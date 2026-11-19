// pintstl.h - the small part of the C++ standard library that the game uses.
//
// The PINT 4.0 SDK (Windows NT 4.0 on ARM) compiles C++14, but it has only the
// C run-time of Windows NT 4.0: no <vector>, <string>, <algorithm> or <cmath>.
// This header gives those parts: std::vector, std::string, std::pair, a few
// algorithms, and the float overloads of the math functions.
#pragma once

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <new.h>
#include <initializer_list>

// ---------------------------------------------------------------------------
// Math. On ARM, the math.h of NT 4.0 has only the double functions. The C++
// overloads keep float math in float, as <cmath> does: abs(float) must not
// call abs(int).

inline float sqrtf(float x) { return static_cast<float>(sqrt(x)); }
inline float sinf(float x) { return static_cast<float>(sin(x)); }
inline float cosf(float x) { return static_cast<float>(cos(x)); }
inline float tanf(float x) { return static_cast<float>(tan(x)); }
inline float asinf(float x) { return static_cast<float>(asin(x)); }
inline float acosf(float x) { return static_cast<float>(acos(x)); }
inline float atanf(float x) { return static_cast<float>(atan(x)); }
inline float atan2f(float y, float x) { return static_cast<float>(atan2(y, x)); }
inline float expf(float x) { return static_cast<float>(exp(x)); }
inline float logf(float x) { return static_cast<float>(log(x)); }
inline float log10f(float x) { return static_cast<float>(log10(x)); }
inline float powf(float x, float y) { return static_cast<float>(pow(x, y)); }
inline float fmodf(float x, float y) { return static_cast<float>(fmod(x, y)); }
inline float fabsf(float x) { return static_cast<float>(fabs(x)); }
inline float floorf(float x) { return static_cast<float>(floor(x)); }
inline float ceilf(float x) { return static_cast<float>(ceil(x)); }
inline double round(double x) { return x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5); }
inline float roundf(float x) { return static_cast<float>(round(static_cast<double>(x))); }

inline float sqrt(float x) { return sqrtf(x); }
inline float sin(float x) { return sinf(x); }
inline float cos(float x) { return cosf(x); }
inline float tan(float x) { return tanf(x); }
inline float asin(float x) { return asinf(x); }
inline float acos(float x) { return acosf(x); }
inline float atan(float x) { return atanf(x); }
inline float exp(float x) { return expf(x); }
inline float log(float x) { return logf(x); }
inline float log10(float x) { return log10f(x); }
inline float fabs(float x) { return fabsf(x); }
inline float floor(float x) { return floorf(x); }
inline float ceil(float x) { return ceilf(x); }
inline float round(float x) { return roundf(x); }
inline float atan2(float y, float x) { return atan2f(y, x); }
inline float pow(float x, float y) { return powf(x, y); }
inline float fmod(float x, float y) { return fmodf(x, y); }
inline float abs(float x) { return fabsf(x); }
inline double abs(double x) { return fabs(x); }
inline long abs(long x) { return labs(x); }

// NAN and isnan are C99: the math.h of NT 4.0 does not have them.
inline float pint_nan()
{
	union
	{
		unsigned int Bits;
		float Value;
	} nan = {0x7FC00000u};
	return nan.Value;
}

#ifndef NAN
#define NAN (pint_nan())
#endif

inline bool isnan(float x) { return x != x; }
inline bool isnan(double x) { return x != x; }

// Integer arguments go to the double functions, as with <cmath>.
template <class T> inline double sqrt(T x) { return sqrt(static_cast<double>(x)); }
template <class T> inline double floor(T x) { return floor(static_cast<double>(x)); }
template <class T> inline double ceil(T x) { return ceil(static_cast<double>(x)); }
template <class T> inline double round(T x) { return round(static_cast<double>(x)); }
template <class A, class B> inline double pow(A x, B y) { return pow(static_cast<double>(x), static_cast<double>(y)); }
template <class A, class B> inline double atan2(A y, B x) { return atan2(static_cast<double>(y), static_cast<double>(x)); }
template <class A, class B> inline double fmod(A x, B y) { return fmod(static_cast<double>(x), static_cast<double>(y)); }

namespace std
{
	using ::size_t;
	using ::ptrdiff_t;

	using ::sqrt;
	using ::sin;
	using ::cos;
	using ::tan;
	using ::asin;
	using ::acos;
	using ::atan;
	using ::atan2;
	using ::exp;
	using ::log;
	using ::log10;
	using ::pow;
	using ::fmod;
	using ::fabs;
	using ::floor;
	using ::ceil;
	using ::round;
	using ::abs;
	using ::isnan;

	using ::memcpy;
	using ::memmove;
	using ::memset;
	using ::memcmp;
	using ::strlen;
	using ::strcmp;
	using ::strncmp;
	using ::strcpy;
	using ::strncpy;
	using ::rand;
	using ::srand;
	using ::exit;
	using ::tolower;
	using ::toupper;

	// -----------------------------------------------------------------------
	// Type helpers. remove_reference comes from <xtr1common> of the compiler,
	// through <initializer_list>.

	template <class T>
	constexpr typename remove_reference<T>::type&& move(T&& value) noexcept
	{
		return static_cast<typename remove_reference<T>::type&&>(value);
	}

	template <class T>
	constexpr T&& forward(typename remove_reference<T>::type& value) noexcept
	{
		return static_cast<T&&>(value);
	}

	template <class T>
	constexpr T&& forward(typename remove_reference<T>::type&& value) noexcept
	{
		return static_cast<T&&>(value);
	}

	template <class Base, class Derived>
	struct is_base_of
	{
		static constexpr bool value = __is_base_of(Base, Derived);
	};

	template <class T>
	void swap(T& a, T& b)
	{
		T tmp(move(a));
		a = move(b);
		b = move(tmp);
	}

	template <class T>
	class reference_wrapper
	{
	public:
		constexpr reference_wrapper(T& ref) : ptr(&ref)
		{
		}

		operator T&() const { return *ptr; }
		T& get() const { return *ptr; }

	private:
		T* ptr;
	};

	template <class A, class B>
	struct pair
	{
		A first;
		B second;

		constexpr pair() : first(), second()
		{
		}

		pair(const A& a, const B& b) : first(a), second(b)
		{
		}

		template <class U, class V>
		pair(U&& a, V&& b) : first(forward<U>(a)), second(forward<V>(b))
		{
		}
	};

	template <class A, class B>
	pair<A, B> make_pair(A a, B b)
	{
		return pair<A, B>(a, b);
	}

	// -----------------------------------------------------------------------
	// min, max and the algorithms

	template <class T>
	constexpr const T& min(const T& a, const T& b)
	{
		return b < a ? b : a;
	}

	template <class T>
	constexpr const T& max(const T& a, const T& b)
	{
		return a < b ? b : a;
	}

	template <class T, class Compare>
	constexpr const T& min(const T& a, const T& b, Compare comp)
	{
		return comp(b, a) ? b : a;
	}

	template <class T, class Compare>
	constexpr const T& max(const T& a, const T& b, Compare comp)
	{
		return comp(a, b) ? b : a;
	}

	template <class It>
	ptrdiff_t distance(It first, It last)
	{
		return last - first;
	}

	template <class It, class T>
	It find(It first, It last, const T& value)
	{
		for (; first != last; ++first)
			if (*first == value)
				return first;
		return last;
	}

	template <class It, class Pred>
	It find_if(It first, It last, Pred pred)
	{
		for (; first != last; ++first)
			if (pred(*first))
				return first;
		return last;
	}

	template <class It, class Compare>
	It min_element(It first, It last, Compare comp)
	{
		It best = first;
		if (first == last)
			return last;
		while (++first != last)
			if (comp(*first, *best))
				best = first;
		return best;
	}

	template <class InIt, class OutIt>
	OutIt copy(InIt first, InIt last, OutIt out)
	{
		for (; first != last; ++first, ++out)
			*out = *first;
		return out;
	}

	template <class InIt, class Size, class OutIt>
	OutIt copy_n(InIt first, Size count, OutIt out)
	{
		for (; count > 0; --count, ++first, ++out)
			*out = *first;
		return out;
	}

	template <class InIt, class OutIt, class Op>
	OutIt transform(InIt first, InIt last, OutIt out, Op op)
	{
		for (; first != last; ++first, ++out)
			*out = op(*first);
		return out;
	}

	template <class It, class T>
	void fill(It first, It last, const T& value)
	{
		for (; first != last; ++first)
			*first = value;
	}

	struct pint_less
	{
		template <class T>
		bool operator()(const T& a, const T& b) const { return a < b; }
	};

	// A stable merge sort. For short ranges it is an insertion sort, as in the
	// std::sort of Microsoft C, so small lists sort in the same order.
	template <class T, class Compare>
	void pint_sort(T* data, T* tmp, size_t count, Compare& comp)
	{
		if (count < 2)
			return;
		if (count <= 32)
		{
			for (size_t i = 1; i < count; i++)
			{
				T value(move(data[i]));
				size_t j = i;
				for (; j > 0 && comp(value, data[j - 1]); j--)
					data[j] = move(data[j - 1]);
				data[j] = move(value);
			}
			return;
		}

		size_t half = count / 2;
		pint_sort(data, tmp, half, comp);
		pint_sort(data + half, tmp, count - half, comp);

		size_t i = 0, j = half, k = 0;
		while (i < half && j < count)
		{
			if (comp(data[j], data[i]))
				tmp[k++] = move(data[j++]);
			else
				tmp[k++] = move(data[i++]);
		}
		while (i < half)
			tmp[k++] = move(data[i++]);
		while (j < count)
			tmp[k++] = move(data[j++]);
		for (k = 0; k < count; k++)
			data[k] = move(tmp[k]);
	}

	template <class T, class Compare>
	void sort(T* first, T* last, Compare comp)
	{
		size_t count = static_cast<size_t>(last - first);
		if (count < 2)
			return;
		T* tmp = count > 32 ? new T[count] : nullptr;
		pint_sort(first, tmp, count, comp);
		delete[] tmp;
	}

	template <class T>
	void sort(T* first, T* last)
	{
		sort(first, last, pint_less());
	}

	// -----------------------------------------------------------------------
	// A reverse iterator over a pointer range.

	template <class T>
	class pint_reverse_iterator
	{
	public:
		explicit pint_reverse_iterator(T* base) : Base(base)
		{
		}

		T& operator*() const { return *(Base - 1); }
		T* operator->() const { return Base - 1; }

		pint_reverse_iterator& operator++()
		{
			--Base;
			return *this;
		}

		pint_reverse_iterator operator++(int)
		{
			pint_reverse_iterator old(*this);
			--Base;
			return old;
		}

		bool operator==(const pint_reverse_iterator& other) const { return Base == other.Base; }
		bool operator!=(const pint_reverse_iterator& other) const { return Base != other.Base; }

	private:
		T* Base;
	};

	// -----------------------------------------------------------------------
	// vector

	template <class T>
	class vector
	{
	public:
		typedef T value_type;
		typedef T* iterator;
		typedef const T* const_iterator;
		typedef pint_reverse_iterator<T> reverse_iterator;
		typedef pint_reverse_iterator<const T> const_reverse_iterator;
		typedef T& reference;
		typedef const T& const_reference;
		typedef size_t size_type;

		constexpr vector() noexcept : Data(nullptr), Count(0), Capacity(0)
		{
		}

		explicit vector(size_t count) : Data(nullptr), Count(0), Capacity(0)
		{
			resize(count);
		}

		vector(size_t count, const T& value) : Data(nullptr), Count(0), Capacity(0)
		{
			resize(count, value);
		}

		vector(std::initializer_list<T> list) : Data(nullptr), Count(0), Capacity(0)
		{
			reserve(list.size());
			for (const T* it = list.begin(); it != list.end(); ++it)
				push_back(*it);
		}

		vector(const vector& other) : Data(nullptr), Count(0), Capacity(0)
		{
			reserve(other.Count);
			for (size_t i = 0; i < other.Count; i++)
				push_back(other.Data[i]);
		}

		vector(vector&& other) noexcept : Data(other.Data), Count(other.Count), Capacity(other.Capacity)
		{
			other.Data = nullptr;
			other.Count = other.Capacity = 0;
		}

		~vector()
		{
			clear();
			free(Data);
		}

		vector& operator=(const vector& other)
		{
			if (this != &other)
			{
				clear();
				reserve(other.Count);
				for (size_t i = 0; i < other.Count; i++)
					push_back(other.Data[i]);
			}
			return *this;
		}

		vector& operator=(vector&& other) noexcept
		{
			swap(other);
			return *this;
		}

		size_t size() const { return Count; }
		size_t capacity() const { return Capacity; }
		bool empty() const { return Count == 0; }

		T& operator[](size_t index) { return Data[index]; }
		const T& operator[](size_t index) const { return Data[index]; }
		T& at(size_t index) { return Data[index]; }
		const T& at(size_t index) const { return Data[index]; }
		T& front() { return Data[0]; }
		const T& front() const { return Data[0]; }
		T& back() { return Data[Count - 1]; }
		const T& back() const { return Data[Count - 1]; }
		T* data() { return Data; }
		const T* data() const { return Data; }

		T* begin() { return Data; }
		T* end() { return Data + Count; }
		const T* begin() const { return Data; }
		const T* end() const { return Data + Count; }
		const T* cbegin() const { return Data; }
		const T* cend() const { return Data + Count; }
		reverse_iterator rbegin() { return reverse_iterator(Data + Count); }
		reverse_iterator rend() { return reverse_iterator(Data); }
		const_reverse_iterator rbegin() const { return const_reverse_iterator(Data + Count); }
		const_reverse_iterator rend() const { return const_reverse_iterator(Data); }

		void reserve(size_t count)
		{
			if (count > Capacity)
				Reallocate(count);
		}

		void shrink_to_fit()
		{
			if (Count == 0)
			{
				free(Data);
				Data = nullptr;
				Capacity = 0;
			}
			else if (Count < Capacity)
			{
				Reallocate(Count);
			}
		}

		void resize(size_t count)
		{
			while (Count > count)
				Data[--Count].~T();
			if (count > Capacity)
				Reallocate(count);
			while (Count < count)
				new(Data + Count++) T();
		}

		void resize(size_t count, const T& value)
		{
			T copy(value);
			while (Count > count)
				Data[--Count].~T();
			if (count > Capacity)
				Reallocate(count);
			while (Count < count)
				new(Data + Count++) T(copy);
		}

		void push_back(const T& value)
		{
			if (Count == Capacity)
			{
				T copy(value);
				Grow(Count + 1);
				new(Data + Count) T(move(copy));
			}
			else
			{
				new(Data + Count) T(value);
			}
			Count++;
		}

		void push_back(T&& value)
		{
			if (Count == Capacity)
			{
				T copy(move(value));
				Grow(Count + 1);
				new(Data + Count) T(move(copy));
			}
			else
			{
				new(Data + Count) T(move(value));
			}
			Count++;
		}

		template <class... Args>
		T& emplace_back(Args&&... args)
		{
			push_back(T(forward<Args>(args)...));
			return back();
		}

		void pop_back()
		{
			Data[--Count].~T();
		}

		void clear()
		{
			while (Count)
				Data[--Count].~T();
		}

		T* erase(const T* position)
		{
			size_t index = static_cast<size_t>(position - Data);
			for (size_t i = index; i + 1 < Count; i++)
				Data[i] = move(Data[i + 1]);
			Data[--Count].~T();
			return Data + index;
		}

		T* erase(const T* first, const T* last)
		{
			size_t index = static_cast<size_t>(first - Data);
			size_t removed = static_cast<size_t>(last - first);
			if (removed)
			{
				for (size_t i = index; i + removed < Count; i++)
					Data[i] = move(Data[i + removed]);
				for (size_t i = 0; i < removed; i++)
					Data[--Count].~T();
			}
			return Data + index;
		}

		T* insert(const T* position, const T& value)
		{
			size_t index = static_cast<size_t>(position - Data);
			T copy(value);
			Grow(Count + 1);
			if (index == Count)
			{
				new(Data + Count) T(move(copy));
			}
			else
			{
				new(Data + Count) T(move(Data[Count - 1]));
				for (size_t i = Count - 1; i > index; i--)
					Data[i] = move(Data[i - 1]);
				Data[index] = move(copy);
			}
			Count++;
			return Data + index;
		}

		// Inserts the range [first, last); the iterators must be pointers or
		// random access iterators.
		template <class It>
		T* insert(const T* position, It first, It last)
		{
			size_t index = static_cast<size_t>(position - Data);
			vector items;
			for (; first != last; ++first)
				items.push_back(*first);
			size_t added = items.Count;
			if (!added)
				return Data + index;

			Grow(Count + added);
			for (size_t i = Count; i > index; i--)
			{
				new(Data + i - 1 + added) T(move(Data[i - 1]));
				Data[i - 1].~T();
			}
			for (size_t i = 0; i < added; i++)
				new(Data + index + i) T(move(items.Data[i]));
			Count += added;
			return Data + index;
		}

		void swap(vector& other) noexcept
		{
			T* data = Data;
			size_t count = Count, capacity = Capacity;
			Data = other.Data;
			Count = other.Count;
			Capacity = other.Capacity;
			other.Data = data;
			other.Count = count;
			other.Capacity = capacity;
		}

	private:
		T* Data;
		size_t Count;
		size_t Capacity;

		void Grow(size_t needed)
		{
			if (needed <= Capacity)
				return;
			size_t capacity = Capacity ? Capacity * 2 : 4;
			if (capacity < needed)
				capacity = needed;
			Reallocate(capacity);
		}

		void Reallocate(size_t capacity)
		{
			T* data = static_cast<T*>(malloc(capacity * sizeof(T)));
			for (size_t i = 0; i < Count; i++)
			{
				new(data + i) T(move(Data[i]));
				Data[i].~T();
			}
			free(Data);
			Data = data;
			Capacity = capacity;
		}
	};

	template <class T, size_t N> T* begin(T (&array)[N]) { return array; }
	template <class T, size_t N> T* end(T (&array)[N]) { return array + N; }
	template <class C> auto begin(C& c) -> decltype(c.begin()) { return c.begin(); }
	template <class C> auto end(C& c) -> decltype(c.end()) { return c.end(); }
	template <class C> auto begin(const C& c) -> decltype(c.begin()) { return c.begin(); }
	template <class C> auto end(const C& c) -> decltype(c.end()) { return c.end(); }

	// -----------------------------------------------------------------------
	// string. An empty string has no buffer; c_str() is then "".

	class string
	{
	public:
		typedef size_t size_type;
		typedef char value_type;
		typedef char* iterator;
		typedef const char* const_iterator;
		static const size_t npos = static_cast<size_t>(-1);

		constexpr string() noexcept : Data(nullptr), Count(0), Capacity(0)
		{
		}

		string(const char* text) : Data(nullptr), Count(0), Capacity(0)
		{
			if (text)
				append(text, strlen(text));
		}

		string(const char* text, size_t count) : Data(nullptr), Count(0), Capacity(0)
		{
			append(text, count);
		}

		string(const char* first, const char* last) : Data(nullptr), Count(0), Capacity(0)
		{
			append(first, static_cast<size_t>(last - first));
		}

		string(size_t count, char ch) : Data(nullptr), Count(0), Capacity(0)
		{
			append(count, ch);
		}

		string(const string& other) : Data(nullptr), Count(0), Capacity(0)
		{
			append(other.Data, other.Count);
		}

		string(string&& other) noexcept : Data(other.Data), Count(other.Count), Capacity(other.Capacity)
		{
			other.Data = nullptr;
			other.Count = other.Capacity = 0;
		}

		~string()
		{
			free(Data);
		}

		string& operator=(const string& other)
		{
			if (this != &other)
				assign(other.Data, other.Count);
			return *this;
		}

		string& operator=(string&& other) noexcept
		{
			swap(other);
			return *this;
		}

		string& operator=(const char* text)
		{
			return assign(text, text ? strlen(text) : 0);
		}

		string& operator=(char ch)
		{
			return assign(&ch, 1);
		}

		string& assign(const char* text, size_t count)
		{
			string tmp(text, count);
			swap(tmp);
			return *this;
		}

		const char* c_str() const { return Data ? Data : ""; }
		const char* data() const { return c_str(); }
		size_t size() const { return Count; }
		size_t length() const { return Count; }
		size_t capacity() const { return Capacity; }
		bool empty() const { return Count == 0; }

		char& operator[](size_t index) { return Data ? Data[index] : EmptyChar(); }
		const char& operator[](size_t index) const { return c_str()[index]; }
		char& at(size_t index) { return (*this)[index]; }
		const char& at(size_t index) const { return c_str()[index]; }
		char& front() { return (*this)[0]; }
		char& back() { return (*this)[Count - 1]; }
		const char& front() const { return c_str()[0]; }
		const char& back() const { return c_str()[Count - 1]; }

		char* begin() { return Data; }
		char* end() { return Data + Count; }
		const char* begin() const { return c_str(); }
		const char* end() const { return c_str() + Count; }

		void clear()
		{
			Count = 0;
			if (Data)
				Data[0] = 0;
		}

		void reserve(size_t count)
		{
			if (count > Capacity)
				Reallocate(count);
		}

		void resize(size_t count, char ch = 0)
		{
			if (count < Count)
			{
				Count = count;
				Data[Count] = 0;
			}
			else
			{
				append(count - Count, ch);
			}
		}

		void push_back(char ch)
		{
			append(&ch, 1);
		}

		void pop_back()
		{
			if (Count)
				Data[--Count] = 0;
		}

		string& append(const char* text, size_t count)
		{
			if (!count)
				return *this;
			if (Data && text >= Data && text < Data + Count)
			{
				string tmp(text, count);
				return append(tmp.Data, tmp.Count);
			}
			Grow(Count + count);
			memcpy(Data + Count, text, count);
			Count += count;
			Data[Count] = 0;
			return *this;
		}

		string& append(size_t count, char ch)
		{
			if (!count)
				return *this;
			Grow(Count + count);
			memset(Data + Count, ch, count);
			Count += count;
			Data[Count] = 0;
			return *this;
		}

		string& append(const char* text) { return append(text, strlen(text)); }
		string& append(const string& other) { return append(other.Data, other.Count); }
		string& operator+=(const string& other) { return append(other.Data, other.Count); }
		string& operator+=(const char* text) { return append(text, strlen(text)); }
		string& operator+=(char ch) { return append(&ch, 1); }

		string& insert(size_t position, const char* text, size_t count)
		{
			if (position > Count)
				position = Count;
			string result(c_str(), position);
			result.append(text, count);
			result.append(c_str() + position, Count - position);
			swap(result);
			return *this;
		}

		string& insert(size_t position, const char* text) { return insert(position, text, strlen(text)); }
		string& insert(size_t position, const string& other) { return insert(position, other.c_str(), other.Count); }

		string& insert(size_t position, size_t count, char ch)
		{
			string chars(count, ch);
			return insert(position, chars.c_str(), chars.Count);
		}

		string& erase(size_t position = 0, size_t count = npos)
		{
			if (position >= Count)
				return *this;
			if (count > Count - position)
				count = Count - position;
			memmove(Data + position, Data + position + count, Count - position - count + 1);
			Count -= count;
			return *this;
		}

		string substr(size_t position = 0, size_t count = npos) const
		{
			if (position > Count)
				position = Count;
			if (count > Count - position)
				count = Count - position;
			return string(c_str() + position, count);
		}

		size_t find(char ch, size_t position = 0) const
		{
			for (size_t i = position; i < Count; i++)
				if (Data[i] == ch)
					return i;
			return npos;
		}

		size_t find(const char* text, size_t position = 0) const
		{
			size_t length = strlen(text);
			if (length == 0)
				return position <= Count ? position : npos;
			for (size_t i = position; i + length <= Count; i++)
				if (memcmp(Data + i, text, length) == 0)
					return i;
			return npos;
		}

		size_t find(const string& other, size_t position = 0) const { return find(other.c_str(), position); }

		size_t rfind(char ch, size_t position = npos) const
		{
			if (!Count)
				return npos;
			size_t i = position < Count ? position : Count - 1;
			for (;; i--)
			{
				if (Data[i] == ch)
					return i;
				if (i == 0)
					return npos;
			}
		}

		int compare(const char* text) const { return strcmp(c_str(), text); }
		int compare(const string& other) const { return strcmp(c_str(), other.c_str()); }

		void swap(string& other) noexcept
		{
			char* data = Data;
			size_t count = Count, capacity = Capacity;
			Data = other.Data;
			Count = other.Count;
			Capacity = other.Capacity;
			other.Data = data;
			other.Count = count;
			other.Capacity = capacity;
		}

	private:
		char* Data;
		size_t Count;
		size_t Capacity;

		static char& EmptyChar()
		{
			static char empty;
			empty = 0;
			return empty;
		}

		void Grow(size_t needed)
		{
			if (needed <= Capacity)
				return;
			size_t capacity = Capacity ? Capacity * 2 : 15;
			if (capacity < needed)
				capacity = needed;
			Reallocate(capacity);
		}

		void Reallocate(size_t capacity)
		{
			char* data = static_cast<char*>(malloc(capacity + 1));
			if (Count)
				memcpy(data, Data, Count);
			data[Count] = 0;
			free(Data);
			Data = data;
			Capacity = capacity;
		}
	};

	inline string operator+(const string& a, const string& b)
	{
		string result(a);
		result += b;
		return result;
	}

	inline string operator+(const string& a, const char* b)
	{
		string result(a);
		result += b;
		return result;
	}

	inline string operator+(const char* a, const string& b)
	{
		string result(a);
		result += b;
		return result;
	}

	inline string operator+(const string& a, char b)
	{
		string result(a);
		result += b;
		return result;
	}

	inline string operator+(string&& a, const string& b)
	{
		a += b;
		return move(a);
	}

	inline string operator+(string&& a, const char* b)
	{
		a += b;
		return move(a);
	}

	inline bool operator==(const string& a, const string& b)
	{
		return a.size() == b.size() && strcmp(a.c_str(), b.c_str()) == 0;
	}

	inline bool operator==(const string& a, const char* b) { return strcmp(a.c_str(), b) == 0; }
	inline bool operator==(const char* a, const string& b) { return strcmp(a, b.c_str()) == 0; }
	inline bool operator!=(const string& a, const string& b) { return !(a == b); }
	inline bool operator!=(const string& a, const char* b) { return !(a == b); }
	inline bool operator!=(const char* a, const string& b) { return !(a == b); }
	inline bool operator<(const string& a, const string& b) { return strcmp(a.c_str(), b.c_str()) < 0; }

	inline string to_string(int value)
	{
		char buffer[16];
		sprintf(buffer, "%d", value);
		return string(buffer);
	}

	inline string to_string(unsigned value)
	{
		char buffer[16];
		sprintf(buffer, "%u", value);
		return string(buffer);
	}

	inline string to_string(long value)
	{
		char buffer[16];
		sprintf(buffer, "%ld", value);
		return string(buffer);
	}

	inline string to_string(unsigned long value)
	{
		char buffer[16];
		sprintf(buffer, "%lu", value);
		return string(buffer);
	}

	inline string to_string(double value)
	{
		char buffer[64];
		_snprintf(buffer, sizeof buffer - 1, "%f", value);
		buffer[sizeof buffer - 1] = 0;
		return string(buffer);
	}

	inline string to_string(float value)
	{
		return to_string(static_cast<double>(value));
	}

	inline int stoi(const string& text)
	{
		return atoi(text.c_str());
	}

	inline float stof(const string& text)
	{
		return static_cast<float>(atof(text.c_str()));
	}
}

// snprintf is C99; the NT 4.0 C run-time has _snprintf, which does not end the
// text with a zero when the buffer is full.
inline int pint_snprintf(char* buffer, size_t size, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = size ? _vsnprintf(buffer, size, format, args) : -1;
	va_end(args);
	if (size && (result < 0 || static_cast<size_t>(result) >= size))
	{
		buffer[size - 1] = 0;
		if (result < 0)
			result = static_cast<int>(size - 1);
	}
	return result;
}

#define snprintf pint_snprintf
