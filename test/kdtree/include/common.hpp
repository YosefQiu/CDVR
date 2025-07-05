#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>

namespace kdTree
{ 
	// ==================================================================
	// Simple vector types 
	// ==================================================================
	struct float2
	{
		float x, y;
		float2() = default;
		float2(float x, float y) : x(x), y(y) {}
	};

	struct float3
	{
		float x, y, z;
		float3() = default;
		float3(float x, float y, float z) : x(x), y(y), z(z) {}
	};

	struct float4
	{
		float x, y, z, w;
		float4() = default;
		float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	};

	struct int2
	{
		int x, y;
		int2() = default;
		int2(int x, int y) : x(x), y(y) {}
	};

	struct int3
	{
		int x, y, z;
		int3() = default;
		int3(int x, int y, int z) : x(x), y(y), z(z) {}
	};

	struct int4
	{
		int x, y, z, w;
		int4() = default;
		int4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}
	};

	// Helper functions to create vectors
	inline float2 make_float2(float x, float y) { return float2(x, y); }
	inline float3 make_float3(float x, float y, float z) { return float3(x, y, z); }
	inline float4 make_float4(float x, float y, float z, float w)
	{
		return float4(x, y, z, w);
	}
	inline int2 make_int2(int x, int y) { return int2(x, y); }
	inline int3 make_int3(int x, int y, int z) { return int3(x, y, z); }
	inline int4 make_int4(int x, int y, int z, int w) { return int4(x, y, z, w); }

	// ==================================================================
	// Type traits
	// ==================================================================

	template <typename vec_t> struct scalar_type_of;
	template <> struct scalar_type_of<float2>
	{
		using type = float;
	};
	template <> struct scalar_type_of<float3>
	{
		using type = float;
	};
	template <> struct scalar_type_of<float4>
	{
		using type = float;
	};
	template <> struct scalar_type_of<int2>
	{
		using type = int;
	};
	template <> struct scalar_type_of<int3>
	{
		using type = int;
	};
	template <> struct scalar_type_of<int4>
	{
		using type = int;
	};

	template <typename vec_t> struct num_dims_of;
	template <> struct num_dims_of<float2>
	{
		enum
		{
			value = 2
		};
	};
	template <> struct num_dims_of<float3>
	{
		enum
		{
			value = 3
		};
	};
	template <> struct num_dims_of<float4>
	{
		enum
		{
			value = 4
		};
	};
	template <> struct num_dims_of<int2>
	{
		enum
		{
			value = 2
		};
	};
	template <> struct num_dims_of<int3>
	{
		enum
		{
			value = 3
		};
	};
	template <> struct num_dims_of<int4>
	{
		enum
		{
			value = 4
		};
	};

	// ==================================================================
	// Coordinate access functions
	// ==================================================================

	inline float get_coord(const float2 &v, int d) { return d ? v.y : v.x; }
	inline float get_coord(const float3 &v, int d)
	{
		return (d == 2) ? v.z : (d ? v.y : v.x);
	}
	inline float get_coord(const float4 &v, int d)
	{
		return (d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x);
	}

	inline float &get_coord(float2 &v, int d) { return d ? v.y : v.x; }
	inline float &get_coord(float3 &v, int d)
	{
		return (d == 2) ? v.z : (d ? v.y : v.x);
	}
	inline float &get_coord(float4 &v, int d)
	{
		return (d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x);
	}

	inline int get_coord(const int2 &v, int d) { return d ? v.y : v.x; }
	inline int get_coord(const int3 &v, int d)
	{
		return (d == 2) ? v.z : (d ? v.y : v.x);
	}
	inline int get_coord(const int4 &v, int d)
	{
		return (d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x);
	}

	inline int &get_coord(int2 &v, int d) { return d ? v.y : v.x; }
	inline int &get_coord(int3 &v, int d)
	{
		return (d == 2) ? v.z : (d ? v.y : v.x);
	}
	inline int &get_coord(int4 &v, int d)
	{
		return (d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x);
	}

	inline void set_coord(int2 &v, int d, int vv) { (d ? v.y : v.x) = vv; }
	inline void set_coord(int3 &v, int d, int vv)
	{
		((d == 2) ? v.z : (d ? v.y : v.x)) = vv;
	}
	inline void set_coord(int4 &v, int d, int vv)
	{
		((d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x)) = vv;
	}

	inline void set_coord(float2 &v, int d, float vv) { (d ? v.y : v.x) = vv; }
	inline void set_coord(float3 &v, int d, float vv)
	{
		((d == 2) ? v.z : (d ? v.y : v.x)) = vv;
	}
	inline void set_coord(float4 &v, int d, float vv)
	{
		((d >= 2) ? (d > 2 ? v.w : v.z) : (d ? v.y : v.x)) = vv;
	}

	// ==================================================================
	// Utility functions
	// ==================================================================

	inline int32_t divRoundUp(int32_t a, int32_t b) { return (a + b - 1) / b; }
	inline uint32_t divRoundUp(uint32_t a, uint32_t b) { return (a + b - 1) / b; }
	inline int64_t divRoundUp(int64_t a, int64_t b) { return (a + b - 1) / b; }
	inline uint64_t divRoundUp(uint64_t a, uint64_t b) { return (a + b - 1) / b; }

	// ==================================================================
	// Vector operations
	// ==================================================================

	inline float2 operator-(float2 a, float2 b)
	{
		return make_float2(a.x - b.x, a.y - b.y);
	}
	inline float3 operator-(float3 a, float3 b)
	{
		return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
	}
	inline float4 operator-(float4 a, float4 b)
	{
		return make_float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
	}

	inline float dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
	inline float dot(float3 a, float3 b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
	inline float dot(float4 a, float4 b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

	inline float2 min(float2 a, float2 b)
	{
		return make_float2(std::min(a.x, b.x), std::min(a.y, b.y));
	}
	inline float3 min(float3 a, float3 b)
	{
		return make_float3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}
	inline float4 min(float4 a, float4 b)
	{
		return make_float4(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z),
						std::min(a.w, b.w));
	}

	inline float2 max(float2 a, float2 b)
	{
		return make_float2(std::max(a.x, b.x), std::max(a.y, b.y));
	}
	inline float3 max(float3 a, float3 b)
	{
		return make_float3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}
	inline float4 max(float4 a, float4 b)
	{
		return make_float4(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z),
						std::max(a.w, b.w));
	}

	inline std::ostream &operator<<(std::ostream &o, float2 v)
	{
		o << "(" << v.x << "," << v.y << ")";
		return o;
	}
	inline std::ostream &operator<<(std::ostream &o, float3 v)
	{
		o << "(" << v.x << "," << v.y << "," << v.z << ")";
		return o;
	}

	// ==================================================================
	// Arbitrary-dimensional vector type
	// ==================================================================

	template <int N> struct vec_float
	{
		float v[N];
	};

	template <int N> struct scalar_type_of<vec_float<N>>
	{
		using type = float;
	};
	template <int N> struct num_dims_of<vec_float<N>>
	{
		enum
		{
			value = N
		};
	};

	template <int N> inline float get_coord(const vec_float<N> &v, int d)
	{
		return v.v[d];
	}
	template <int N> inline float &get_coord(vec_float<N> &v, int d)
	{
		return v.v[d];
	}
	template <int N> inline void set_coord(vec_float<N> &v, int d, float vv)
	{
		v.v[d] = vv;
	}

	template <int N> inline vec_float<N> min(vec_float<N> a, vec_float<N> b)
	{
		vec_float<N> r;
		for (int i = 0; i < N; i++)
			r.v[i] = min(a.v[i], b.v[i]);
		return r;
	}

	template <int N> inline vec_float<N> max(vec_float<N> a, vec_float<N> b)
	{
		vec_float<N> r;
		for (int i = 0; i < N; i++)
			r.v[i] = max(a.v[i], b.v[i]);
		return r;
	}

	template <int N> inline float dot(vec_float<N> a, vec_float<N> b)
	{
		float sum = 0.f;
		for (int i = 0; i < N; i++)
			sum += a.v[i] * b.v[i];
		return sum;
	}

	template <int N>
	inline vec_float<N> operator-(const vec_float<N> &a, const vec_float<N> &b)
	{
		vec_float<N> r;
		for (int i = 0; i < N; i++)
			r.v[i] = a.v[i] - b.v[i];
		return r;
	}

	// ==================================================================
	// Helper functions for type conversion
	// ==================================================================

	template <typename T> inline float as_float_rz(T t);
	template <> inline float as_float_rz(float f) { return f; }
	template <> inline float as_float_rz(int i) { return static_cast<float>(i); }

	// ==================================================================
	// Distance functions
	// ==================================================================

	template <typename point_t>
	inline float fSqrDistance(const point_t &a, const point_t &b)
	{
		const point_t diff = b - a;
		return as_float_rz(dot(diff, diff));
	}

	template <typename point_t>
	inline auto sqrDistance(const point_t &a, const point_t &b)
	{
		const point_t d = a - b;
		return dot(d, d);
	}

	inline float square_root(float f) { return std::sqrt(f); }

	template <typename point_t>
	inline auto distance(const point_t &a, const point_t &b)
	{
		return square_root(sqrDistance(a, b));
	}

	// ==================================================================
	// Utility functions
	// ==================================================================

	template <typename point_t> inline int arg_max(point_t p)
	{
		enum
		{
			num_dims = num_dims_of<point_t>::value
		};
		using scalar_t = typename scalar_type_of<point_t>::type;
		int best_dim = 0;
		scalar_t best_val = get_coord(p, 0);
		for (int i = 1; i < num_dims; i++)
		{
			scalar_t f = get_coord(p, i);
			if (f > best_val)
			{
				best_val = f;
				best_dim = i;
			}
		}
		return best_dim;
	}

	template <typename scalar_t> inline auto sqr(scalar_t f) { return f * f; }

	template <typename scalar_t> inline scalar_t sqrt(scalar_t f);

	template <> inline float sqrt(float f) { return std::sqrt(f); }

	// ==================================================================
	// Point traits
	// ==================================================================

	template <typename T> struct point_traits;

	template <typename point_t> struct point_traits
	{
		enum
		{
			num_dims = num_dims_of<point_t>::value
		};
		using scalar_t = typename scalar_type_of<point_t>::type;

		static inline scalar_t get_coord(const point_t &v, int d)
		{
			return kdTree::get_coord(v, d);
		}
		static inline scalar_t &get_coord(point_t &v, int d)
		{
			return kdTree::get_coord(v, d);
		}
		static inline void set_coord(point_t &v, int d, scalar_t vv)
		{
			kdTree::set_coord(v, d, vv);
		}
	};

	template <typename T> inline void cukd_swap(T &a, T &b)
	{
		T c = a;
		a = b;
		b = c;
	}

	template <typename data_t, typename data_traits, bool has_dim>
	struct if_has_dims;

	template <typename data_t, typename data_traits>
	struct if_has_dims<data_t, data_traits, false>
	{
		static inline void set_dim(data_t &t, int dim) {}
		static inline int get_dim(const data_t &t, int value_if_false)
		{
			return value_if_false;
		}
	};

	template <typename data_t, typename data_traits>
	struct if_has_dims<data_t, data_traits, true>
	{
		static inline void set_dim(data_t &t, int dim)
		{
			data_traits::set_dim(t, dim);
		}
		static inline int get_dim(const data_t &t, int /* ignore: value_if_false */)
		{
			return data_traits::get_dim(t);
		}
	};

	template<typename _point_t, typename _point_traits=point_traits<_point_t>>
	struct default_data_traits 
	{
		using point_t      = _point_t;
		using point_traits = _point_traits;
		using data_t = _point_t;
	private:
		using scalar_t  = typename point_traits::scalar_t;
	public:    
		static inline const point_t &get_point(const data_t &n) { return n; }
		static inline scalar_t get_coord(const data_t &n, int d) { return point_traits::get_coord(get_point(n),d); }
		enum { has_explicit_dim = false };
		static inline int  get_dim(const data_t &) { return -1; }
		static inline void set_dim(data_t &, int) {}
	};
}

