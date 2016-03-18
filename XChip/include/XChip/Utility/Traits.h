#ifndef _XCHIP_TRAITS_H_
#define _XCHIP_TRAITS_H_
#include <type_traits>

namespace xcpu { namespace utility {

	template<bool cond, class T>
	using enable_if_t = typename std::enable_if<cond, T>::type;
		
	template<class T>
	using underlying_type_t = typename std::underlying_type<T>::type;


	template<class T>
	inline underlying_type_t<T> toUType(T t)
	{
		return static_cast<underlying_type_t<T>>(t);
	}

}}
#endif