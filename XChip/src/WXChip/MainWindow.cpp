/*

WXChip - chip8 emulator using XChip library and a wxWidgets gui.
Copyright (C) 2016  Jared Bruni, Rafael Moura.


This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/gpl-3.0.html.

*/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>
#include <WXChip/dirent.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <dirent.h>
#endif


#include <iostream>
#include <fstream>
#include <regex>
#include <stdexcept>

#include <Utix/ScopeExit.h>
#include <Utix/Common.h>
#include <Utix/CliOpts.h>
#include <Utix/Log.h>
#include <Utix/Memory.h>

#include <WXChip/Main.h>
#include <WXChip/SaveList.h>
#include <WXChip/MainWindow.h>



// local functions declarations
namespace {
static void FillRomPath(const wxString& dirPath, const wxString& filename, std::string* const dest);
static void FillRomPath(const wxString& fullPath, std::string* const dest);
}


wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_MENU(wxID_EXIT, MainWindow::OnExit)
EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
EVT_MENU(ID_MENU_BAR_LOAD_ROM, MainWindow::OnMenuBarLoadRom)
EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
EVT_BUTTON(ID_BUTTON_LOAD_ROM, MainWindow::OnButtonLoadRom)
EVT_BUTTON(ID_BUTTON_SELECT_DIR, MainWindow::OnButtonSelectDir)
EVT_BUTTON(ID_BUTTON_SETTINGS, MainWindow::OnButtonSettings)
EVT_CLOSE(MainWindow::OnClose)
wxEND_EVENT_TABLE()




constexpr const char* const MainWindow::default_emuapp_relative_path;




MainWindow::MainWindow(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(nullptr, 0, title, pos, size, wxCAPTION | wxSYSTEM_MENU | wxMINIMIZE_BOX | wxCLOSE_BOX)
{
	utix::Log("Constructing WXChip MainWindow");
	CreateStatusBar();
	SetStatusText("Welcome to WXChip");
	ComputeEmuAppPath();
	CreateControls();
}


MainWindow::~MainWindow()
{
	utix::Log("Destroying MainWindow...");
	StopEmulator();
}



void MainWindow::CreateControls()
{
	using utix::make_unique;

	CreateMenuBar();

	_panel = make_unique<wxPanel>(this, wxID_ANY);

	_settingsWin = make_unique<SettingsWindow>(this, "WXChip - Settings", wxPoint(150, 150));


	_romsTxt = make_unique<wxStaticText>(_panel.get(), ID_ROMS_TEXT, _T("Roms"), 
                                          wxPoint(10, 10), wxSize(100, 25));

	_listBox = make_unique<wxListBox>(_panel.get(), ID_LISTBOX, wxPoint(10, 35), wxSize(620, 360), 
                                       0, nullptr, wxLB_SINGLE);

	_listBox->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(MainWindow::OnLDown), NULL, this);


	_buttonLoadRom = make_unique<wxButton>(_panel.get(), ID_BUTTON_LOAD_ROM, _T("Load Rom"), 
                                           wxPoint(10, 400), wxSize(100, 35));

	_buttonSelectDir = make_unique<wxButton>(_panel.get(), ID_BUTTON_SELECT_DIR, _T("Select Directory"), 
                                           wxPoint(120, 400), wxSize(110, 35));

	_buttonSettings = make_unique<wxButton>(_panel.get(), ID_BUTTON_SETTINGS, _T("Settings"), 
                                                   wxPoint(240, 400), wxSize(100, 35));
}





void MainWindow::CreateMenuBar()
{
	using utix::make_unique;
	auto menuFile = make_unique<wxMenu>();

	menuFile->Append(ID_MENU_BAR_LOAD_ROM, "&Load Rom...\tCtrl-L", "Load a game rom");
	menuFile->AppendSeparator();
	menuFile->Append(wxID_EXIT);

	auto menuHelp = make_unique<wxMenu>();
	menuHelp->Append(wxID_ABOUT);


	auto menuBar = make_unique<wxMenuBar>();

	if (!menuBar->Append(menuFile.get(), "&File") 
		|| !menuBar->Append(menuHelp.release(), "&Help")) {
		throw std::runtime_error("could not append a menu into wxMenuBar");
	}

	menuFile.release();
	SetMenuBar(menuBar.release());
}






void MainWindow::StartEmulator()
{
	StopEmulator();
	if(!_process.Run(_emuApp + " -ROM " + _romPath + ' ' + _settingsWin->GetArguments()))
		throw std::runtime_error(utix::GetLastLogError());
}




void MainWindow::StopEmulator()
{
	if (_process.IsRunning())
		_process.Terminate();
}




void MainWindow::LoadList(const std::string &dirPath)
{
	using namespace utix;

	if (dirPath == "nopath" || dirPath == _settingsWin->GetDirPath())
		return;

	DIR *dir = opendir(dirPath.c_str());

	if (dir == nullptr)
	{
		LogError("Error could not open directory.");
		return;
	}
	// close the dir in every exit path from this function
	const auto cleanup = make_scope_exit([&dir]() noexcept { closedir(dir); });	
	
	wxArrayString dirFiles;
	dirent *e;

	while ((e = readdir(dir)) != nullptr)
	{
		if (e->d_type == DT_REG)
		{
			std::string file = e->d_name;
			std::regex exp1("ch8$", std::regex_constants::icase);
			std::regex exp2("([0-9a-zA-Z_\\ ]+)", std::regex_constants::icase);
			// if found in 1 regex search, avoid doing the other search
			bool isTag = std::regex_search(file, exp1) || std::regex_match(file, exp2);
			if (isTag) 
				dirFiles.Add(wxString(e->d_name));
		}
	}


	if(!dirFiles.IsEmpty())
	{
		_listBox->Clear();
		_listBox->InsertItems(dirFiles,0);
		_settingsWin->SetDirPath(dirPath);
	}
}



void MainWindow::ComputeEmuAppPath()
{
	_emuApp = utix::GetFullProcDir() + default_emuapp_relative_path;

	if (!std::ifstream(_emuApp).good())
		throw std::runtime_error("Could not find EmuApp executable!");

	// insert quotes around the computed path
	_emuApp.insert(0, "\"");
	_emuApp += "\"";

	utix::Log("_emuApp after compute: %s", _emuApp.c_str());
}





void MainWindow::OnExit(wxCommandEvent&)
{
	Close(true);
}


void MainWindow::OnClose(wxCloseEvent&)
{	
	_settingsWin.release()->Destroy();
	_buttonSettings.release()->Destroy();
	_buttonSelectDir.release()->Destroy();
	_buttonLoadRom.release()->Destroy();
	_listBox.release()->Destroy();
	_romsTxt.release()->Destroy();
	_panel.release()->Destroy();
	Destroy();
}



void MainWindow::OnAbout(wxCommandEvent&)
{
	wxMessageBox("WXChip - wxWidgets GUI for XChip",
		"About WXChip", wxOK | wxICON_INFORMATION);
}



void MainWindow::OnLDown(wxMouseEvent& event)
{
	auto m_lbox = static_cast<wxListBox*>(event.GetEventObject());
	int item = m_lbox->HitTest(event.GetPosition());

	if (item != wxNOT_FOUND)
	{
		FillRomPath(_settingsWin->GetDirPath(), m_lbox->GetString(item), &_romPath);
		utix::Log("Start Rom At Path: %s", _romPath.c_str());
		StartEmulator();
	}
}




void MainWindow::OnButtonSettings(wxCommandEvent&)
{
	_settingsWin->Show(true);
}




void MainWindow::OnButtonLoadRom(wxCommandEvent&)
{
	utix::Log("Starting Rom...");
	StartEmulator();
}




void MainWindow::OnButtonSelectDir(wxCommandEvent&)
{
	wxDirDialog dirDlg(this, "Choose Roms Directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dirDlg.ShowModal() == wxID_OK)
		LoadList(std::string(dirDlg.GetPath().c_str()));
}




void MainWindow::OnMenuBarLoadRom(wxCommandEvent&)
{
	wxFileDialog fdlg(this, "Select Rom", "", "", "All Files (*)|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (fdlg.ShowModal() == wxID_OK) {
		FillRomPath(fdlg.GetPath(), &_romPath);
		utix::Log("Selected File: %s", _romPath.c_str());
		StartEmulator();
	}

}




 // local functiosn definitions
namespace {


static void FillRomPath(const wxString& dirPath, const wxString& filename, std::string* const dest)
{
#ifdef _WIN32
		constexpr char dirSlash =  '\\';
#elif defined(__APPLE__) || defined(__linux__)
		constexpr char dirSlash = '/';
#endif
	((*dest = '\"') += dirPath);

	// check if dirPath had the last dirSlash
	const auto slashIdx =  dest->size() - 1;
	if(dest->at(slashIdx) != dirSlash)
		*dest += dirSlash;

	(*dest += filename) += '\"';
}
static void FillRomPath(const wxString& fullPath, std::string* const dest)
{
	((*dest = '\"') += fullPath) += '\"';
}

}


