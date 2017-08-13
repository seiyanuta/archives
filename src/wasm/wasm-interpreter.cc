// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/ast-decoder.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-module.h"

#include "src/base/accounting-allocator.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

#if DEBUG
#define TRACE(...)                                        \
  do {                                                    \
    if (FLAG_trace_wasm_interpreter) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

#define FOREACH_INTERNAL_OPCODE(V) V(Breakpoint, 0xFF)

#define FOREACH_SIMPLE_BINOP(V) \
  V(I32Add, uint32_t, +)        \
  V(I32Sub, uint32_t, -)        \
  V(I32Mul, uint32_t, *)        \
  V(I32And, uint32_t, &)        \
  V(I32Ior, uint32_t, |)        \
  V(I32Xor, uint32_t, ^)        \
  V(I32Eq, uint32_t, ==)        \
  V(I32Ne, uint32_t, !=)        \
  V(I32LtU, uint32_t, <)        \
  V(I32LeU, uint32_t, <=)       \
  V(I32GtU, uint32_t, >)        \
  V(I32GeU, uint32_t, >=)       \
  V(I32LtS, int32_t, <)         \
  V(I32LeS, int32_t, <=)        \
  V(I32GtS, int32_t, >)         \
  V(I32GeS, int32_t, >=)        \
  V(I64Add, uint64_t, +)        \
  V(I64Sub, uint64_t, -)        \
  V(I64Mul, uint64_t, *)        \
  V(I64And, uint64_t, &)        \
  V(I64Ior, uint64_t, |)        \
  V(I64Xor, uint64_t, ^)        \
  V(I64Eq, uint64_t, ==)        \
  V(I64Ne, uint64_t, !=)        \
  V(I64LtU, uint64_t, <)        \
  V(I64LeU, uint64_t, <=)       \
  V(I64GtU, uint64_t, >)        \
  V(I64GeU, uint64_t, >=)       \
  V(I64LtS, int64_t, <)         \
  V(I64LeS, int64_t, <=)        \
  V(I64GtS, int64_t, >)         \
  V(I64GeS, int64_t, >=)        \
  V(F32Add, float, +)           \
  V(F32Mul, float, *)           \
  V(F32Div, float, /)           \
  V(F32Eq, float, ==)           \
  V(F32Ne, float, !=)           \
  V(F32Lt, float, <)            \
  V(F32Le, float, <=)           \
  V(F32Gt, float, >)            \
  V(F32Ge, float, >=)           \
  V(F64Add, double, +)          \
  V(F64Mul, double, *)          \
  V(F64Div, double, /)          \
  V(F64Eq, double, ==)          \
  V(F64Ne, double, !=)          \
  V(F64Lt, double, <)           \
  V(F64Le, double, <=)          \
  V(F64Gt, double, >)           \
  V(F64Ge, double, >=)

#define FOREACH_OTHER_BINOP(V) \
  V(I32DivS, int32_t)          \
  V(I32DivU, uint32_t)         \
  V(I32RemS, int32_t)          \
  V(I32RemU, uint32_t)         \
  V(I32Shl, uint32_t)          \
  V(I32ShrU, uint32_t)         \
  V(I32ShrS, int32_t)          \
  V(I64DivS, int64_t)          \
  V(I64DivU, uint64_t)         \
  V(I64RemS, int64_t)          \
  V(I64RemU, uint64_t)         \
  V(I64Shl, uint64_t)          \
  V(I64ShrU, uint64_t)         \
  V(I64ShrS, int64_t)          \
  V(I32Ror, int32_t)           \
  V(I32Rol, int32_t)           \
  V(I64Ror, int64_t)           \
  V(I64Rol, int64_t)           \
  V(F32Sub, float)             \
  V(F32Min, float)             \
  V(F32Max, float)             \
  V(F32CopySign, float)        \
  V(F64Min, double)            \
  V(F64Max, double)            \
  V(F64Sub, double)            \
  V(F64CopySign, double)       \
  V(I32AsmjsDivS, int32_t)     \
  V(I32AsmjsDivU, uint32_t)    \
  V(I32AsmjsRemS, int32_t)     \
  V(I32AsmjsRemU, uint32_t)

#define FOREACH_OTHER_UNOP(V)    \
  V(I32Clz, uint32_t)            \
  V(I32Ctz, uint32_t)            \
  V(I32Popcnt, uint32_t)         \
  V(I32Eqz, uint32_t)            \
  V(I64Clz, uint64_t)            \
  V(I64Ctz, uint64_t)            \
  V(I64Popcnt, uint64_t)         \
  V(I64Eqz, uint64_t)            \
  V(F32Abs, float)               \
  V(F32Neg, float)               \
  V(F32Ceil, float)              \
  V(F32Floor, float)             \
  V(F32Trunc, float)             \
  V(F32NearestInt, float)        \
  V(F32Sqrt, float)              \
  V(F64Abs, double)              \
  V(F64Neg, double)              \
  V(F64Ceil, double)             \
  V(F64Floor, double)            \
  V(F64Trunc, double)            \
  V(F64NearestInt, double)       \
  V(F64Sqrt, double)             \
  V(I32SConvertF32, float)       \
  V(I32SConvertF64, double)      \
  V(I32UConvertF32, float)       \
  V(I32UConvertF64, double)      \
  V(I32ConvertI64, int64_t)      \
  V(I64SConvertF32, float)       \
  V(I64SConvertF64, double)      \
  V(I64UConvertF32, float)       \
  V(I64UConvertF64, double)      \
  V(I64SConvertI32, int32_t)     \
  V(I64UConvertI32, uint32_t)    \
  V(F32SConvertI32, int32_t)     \
  V(F32UConvertI32, uint32_t)    \
  V(F32SConvertI64, int64_t)     \
  V(F32UConvertI64, uint64_t)    \
  V(F32ConvertF64, double)       \
  V(F32ReinterpretI32, int32_t)  \
  V(F64SConvertI32, int32_t)     \
  V(F64UConvertI32, uint32_t)    \
  V(F64SConvertI64, int64_t)     \
  V(F64UConvertI64, uint64_t)    \
  V(F64ConvertF32, float)        \
  V(F64ReinterpretI64, int64_t)  \
  V(I32ReinterpretF32, float)    \
  V(I64ReinterpretF64, double)   \
  V(I32AsmjsSConvertF32, float)  \
  V(I32AsmjsUConvertF32, float)  \
  V(I32AsmjsSConvertF64, double) \
  V(I32AsmjsUConvertF64, double)

static inline int32_t ExecuteI32DivS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

static inline uint32_t ExecuteI32DivU(uint32_t a, uint32_t b,
                                      TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

static inline int32_t ExecuteI32RemS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

static inline uint32_t ExecuteI32RemU(uint32_t a, uint32_t b,
                                      TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

static inline uint32_t ExecuteI32Shl(uint32_t a, uint32_t b, TrapReason* trap) {
  return a << (b & 0x1f);
}

static inline uint32_t ExecuteI32ShrU(uint32_t a, uint32_t b,
                                      TrapReason* trap) {
  return a >> (b & 0x1f);
}

static inline int32_t ExecuteI32ShrS(int32_t a, int32_t b, TrapReason* trap) {
  return a >> (b & 0x1f);
}

static inline int64_t ExecuteI64DivS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int64_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

static inline uint64_t ExecuteI64DivU(uint64_t a, uint64_t b,
                                      TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

static inline int64_t ExecuteI64RemS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

static inline uint64_t ExecuteI64RemU(uint64_t a, uint64_t b,
                                      TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

static inline uint64_t ExecuteI64Shl(uint64_t a, uint64_t b, TrapReason* trap) {
  return a << (b & 0x3f);
}

static inline uint64_t ExecuteI64ShrU(uint64_t a, uint64_t b,
                                      TrapReason* trap) {
  return a >> (b & 0x3f);
}

static inline int64_t ExecuteI64ShrS(int64_t a, int64_t b, TrapReason* trap) {
  return a >> (b & 0x3f);
}

static inline uint32_t ExecuteI32Ror(uint32_t a, uint32_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x1f);
  return (a >> shift) | (a << (32 - shift));
}

static inline uint32_t ExecuteI32Rol(uint32_t a, uint32_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x1f);
  return (a << shift) | (a >> (32 - shift));
}

static inline uint64_t ExecuteI64Ror(uint64_t a, uint64_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x3f);
  return (a >> shift) | (a << (64 - shift));
}

static inline uint64_t ExecuteI64Rol(uint64_t a, uint64_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x3f);
  return (a << shift) | (a >> (64 - shift));
}

static float quiet(float a) {
  static const uint32_t kSignalingBit = 1 << 22;
  uint32_t q = bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
  if ((q & kSignalingBit) != 0) {
    // On some machines, the signaling bit set indicates it's a quiet NaN.
    return bit_cast<float>(bit_cast<uint32_t>(a) | kSignalingBit);
  } else {
    // On others, the signaling bit set indicates it's a signaling NaN.
    return bit_cast<float>(bit_cast<uint32_t>(a) & ~kSignalingBit);
  }
}

static double quiet(double a) {
  static const uint64_t kSignalingBit = 1ULL << 51;
  uint64_t q = bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN());
  if ((q & kSignalingBit) != 0) {
    // On some machines, the signaling bit set indicates it's a quiet NaN.
    return bit_cast<double>(bit_cast<uint64_t>(a) | kSignalingBit);
  } else {
    // On others, the signaling bit set indicates it's a signaling NaN.
    return bit_cast<double>(bit_cast<uint64_t>(a) & ~kSignalingBit);
  }
}

static inline float ExecuteF32Sub(float a, float b, TrapReason* trap) {
  float result = a - b;
  // Some architectures (e.g. MIPS) need extra checking to preserve the payload
  // of a NaN operand.
  if (result - result != 0) {
    if (std::isnan(a)) return quiet(a);
    if (std::isnan(b)) return quiet(b);
  }
  return result;
}

static inline float ExecuteF32Min(float a, float b, TrapReason* trap) {
  if (std::isnan(a)) return quiet(a);
  if (std::isnan(b)) return quiet(b);
  return std::min(a, b);
}

static inline float ExecuteF32Max(float a, float b, TrapReason* trap) {
  if (std::isnan(a)) return quiet(a);
  if (std::isnan(b)) return quiet(b);
  return std::max(a, b);
}

static inline float ExecuteF32CopySign(float a, float b, TrapReason* trap) {
  return copysignf(a, b);
}

static inline double ExecuteF64Sub(double a, double b, TrapReason* trap) {
  double result = a - b;
  // Some architectures (e.g. MIPS) need extra checking to preserve the payload
  // of a NaN operand.
  if (result - result != 0) {
    if (std::isnan(a)) return quiet(a);
    if (std::isnan(b)) return quiet(b);
  }
  return result;
}

static inline double ExecuteF64Min(double a, double b, TrapReason* trap) {
  if (std::isnan(a)) return quiet(a);
  if (std::isnan(b)) return quiet(b);
  return std::min(a, b);
}

static inline double ExecuteF64Max(double a, double b, TrapReason* trap) {
  if (std::isnan(a)) return quiet(a);
  if (std::isnan(b)) return quiet(b);
  return std::max(a, b);
}

static inline double ExecuteF64CopySign(double a, double b, TrapReason* trap) {
  return copysign(a, b);
}

static inline int32_t ExecuteI32AsmjsDivS(int32_t a, int32_t b,
                                          TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    return std::numeric_limits<int32_t>::min();
  }
  return a / b;
}

static inline uint32_t ExecuteI32AsmjsDivU(uint32_t a, uint32_t b,
                                           TrapReason* trap) {
  if (b == 0) return 0;
  return a / b;
}

static inline int32_t ExecuteI32AsmjsRemS(int32_t a, int32_t b,
                                          TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1) return 0;
  return a % b;
}

static inline uint32_t ExecuteI32AsmjsRemU(uint32_t a, uint32_t b,
                                           TrapReason* trap) {
  if (b == 0) return 0;
  return a % b;
}

static inline int32_t ExecuteI32AsmjsSConvertF32(float a, TrapReason* trap) {
  return DoubleToInt32(a);
}

static inline uint32_t ExecuteI32AsmjsUConvertF32(float a, TrapReason* trap) {
  return DoubleToUint32(a);
}

static inline int32_t ExecuteI32AsmjsSConvertF64(double a, TrapReason* trap) {
  return DoubleToInt32(a);
}

static inline uint32_t ExecuteI32AsmjsUConvertF64(double a, TrapReason* trap) {
  return DoubleToUint32(a);
}

static int32_t ExecuteI32Clz(uint32_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros32(val);
}

static uint32_t ExecuteI32Ctz(uint32_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros32(val);
}

static uint32_t ExecuteI32Popcnt(uint32_t val, TrapReason* trap) {
  return word32_popcnt_wrapper(&val);
}

static inline uint32_t ExecuteI32Eqz(uint32_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

static int64_t ExecuteI64Clz(uint64_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros64(val);
}

static inline uint64_t ExecuteI64Ctz(uint64_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros64(val);
}

static inline int64_t ExecuteI64Popcnt(uint64_t val, TrapReason* trap) {
  return word64_popcnt_wrapper(&val);
}

static inline int32_t ExecuteI64Eqz(uint64_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

static inline float ExecuteF32Abs(float a, TrapReason* trap) {
  return bit_cast<float>(bit_cast<uint32_t>(a) & 0x7fffffff);
}

static inline float ExecuteF32Neg(float a, TrapReason* trap) {
  return bit_cast<float>(bit_cast<uint32_t>(a) ^ 0x80000000);
}

static inline float ExecuteF32Ceil(float a, TrapReason* trap) {
  return ceilf(a);
}

static inline float ExecuteF32Floor(float a, TrapReason* trap) {
  return floorf(a);
}

static inline float ExecuteF32Trunc(float a, TrapReason* trap) {
  return truncf(a);
}

static inline float ExecuteF32NearestInt(float a, TrapReason* trap) {
  return nearbyintf(a);
}

static inline float ExecuteF32Sqrt(float a, TrapReason* trap) {
  return sqrtf(a);
}

static inline double ExecuteF64Abs(double a, TrapReason* trap) {
  return bit_cast<double>(bit_cast<uint64_t>(a) & 0x7fffffffffffffff);
}

static inline double ExecuteF64Neg(double a, TrapReason* trap) {
  return bit_cast<double>(bit_cast<uint64_t>(a) ^ 0x8000000000000000);
}

static inline double ExecuteF64Ceil(double a, TrapReason* trap) {
  return ceil(a);
}

static inline double ExecuteF64Floor(double a, TrapReason* trap) {
  return floor(a);
}

static inline double ExecuteF64Trunc(double a, TrapReason* trap) {
  return trunc(a);
}

static inline double ExecuteF64NearestInt(double a, TrapReason* trap) {
  return nearbyint(a);
}

static inline double ExecuteF64Sqrt(double a, TrapReason* trap) {
  return sqrt(a);
}

static int32_t ExecuteI32SConvertF32(float a, TrapReason* trap) {
  // The upper bound is (INT32_MAX + 1), which is the lowest float-representable
  // number above INT32_MAX which cannot be represented as int32.
  float upper_bound = 2147483648.0f;
  // We use INT32_MIN as a lower bound because (INT32_MIN - 1) is not
  // representable as float, and no number between (INT32_MIN - 1) and INT32_MIN
  // is.
  float lower_bound = static_cast<float>(INT32_MIN);
  if (a < upper_bound && a >= lower_bound) {
    return static_cast<int32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

static int32_t ExecuteI32SConvertF64(double a, TrapReason* trap) {
  // The upper bound is (INT32_MAX + 1), which is the lowest double-
  // representable number above INT32_MAX which cannot be represented as int32.
  double upper_bound = 2147483648.0;
  // The lower bound is (INT32_MIN - 1), which is the greatest double-
  // representable number below INT32_MIN which cannot be represented as int32.
  double lower_bound = -2147483649.0;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<int32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

static uint32_t ExecuteI32UConvertF32(float a, TrapReason* trap) {
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // float-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0f;
  double lower_bound = -1.0f;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<uint32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

static uint32_t ExecuteI32UConvertF64(double a, TrapReason* trap) {
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // double-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0;
  double lower_bound = -1.0;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<uint32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

static inline uint32_t ExecuteI32ConvertI64(int64_t a, TrapReason* trap) {
  return static_cast<uint32_t>(a & 0xFFFFFFFF);
}

static int64_t ExecuteI64SConvertF32(float a, TrapReason* trap) {
  int64_t output;
  if (!float32_to_int64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

static int64_t ExecuteI64SConvertF64(double a, TrapReason* trap) {
  int64_t output;
  if (!float64_to_int64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

static uint64_t ExecuteI64UConvertF32(float a, TrapReason* trap) {
  uint64_t output;
  if (!float32_to_uint64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

static uint64_t ExecuteI64UConvertF64(double a, TrapReason* trap) {
  uint64_t output;
  if (!float64_to_uint64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

static inline int64_t ExecuteI64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<int64_t>(a);
}

static inline int64_t ExecuteI64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<uint64_t>(a);
}

static inline float ExecuteF32SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

static inline float ExecuteF32UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

static inline float ExecuteF32SConvertI64(int64_t a, TrapReason* trap) {
  float output;
  int64_to_float32_wrapper(&a, &output);
  return output;
}

static inline float ExecuteF32UConvertI64(uint64_t a, TrapReason* trap) {
  float output;
  uint64_to_float32_wrapper(&a, &output);
  return output;
}

static inline float ExecuteF32ConvertF64(double a, TrapReason* trap) {
  return static_cast<float>(a);
}

static inline float ExecuteF32ReinterpretI32(int32_t a, TrapReason* trap) {
  return bit_cast<float>(a);
}

static inline double ExecuteF64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

static inline double ExecuteF64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

static inline double ExecuteF64SConvertI64(int64_t a, TrapReason* trap) {
  double output;
  int64_to_float64_wrapper(&a, &output);
  return output;
}

static inline double ExecuteF64UConvertI64(uint64_t a, TrapReason* trap) {
  double output;
  uint64_to_float64_wrapper(&a, &output);
  return output;
}

static inline double ExecuteF64ConvertF32(float a, TrapReason* trap) {
  return static_cast<double>(a);
}

static inline double ExecuteF64ReinterpretI64(int64_t a, TrapReason* trap) {
  return bit_cast<double>(a);
}

static inline int32_t ExecuteI32ReinterpretF32(float a, TrapReason* trap) {
  return bit_cast<int32_t>(a);
}

static inline int64_t ExecuteI64ReinterpretF64(double a, TrapReason* trap) {
  return bit_cast<int64_t>(a);
}

enum InternalOpcode {
#define DECL_INTERNAL_ENUM(name, value) kInternal##name = value,
  FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_ENUM)
#undef DECL_INTERNAL_ENUM
};

static const char* OpcodeName(uint32_t val) {
  switch (val) {
#define DECL_INTERNAL_CASE(name, value) \
  case kInternal##name:                 \
    return "Internal" #name;
    FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_CASE)
#undef DECL_INTERNAL_CASE
  }
  return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(val));
}

static const int kRunSteps = 1000;

// A helper class to compute the control transfers for each bytecode offset.
// Control transfers allow Br, BrIf, BrTable, If, Else, and End bytecodes to
// be directly executed without the need to dynamically track blocks.
class ControlTransfers : public ZoneObject {
 public:
  ControlTransferMap map_;

  ControlTransfers(Zone* zone, size_t locals_encoded_size, const byte* start,
                   const byte* end)
      : map_(zone) {
    // A control reference including from PC, from value depth, and whether
    // a value is explicitly passed (e.g. br/br_if/br_table with value).
    struct CRef {
      const byte* pc;
      sp_t value_depth;
      bool explicit_value;
    };

    // Represents a control flow label.
    struct CLabel : public ZoneObject {
      const byte* target;
      size_t value_depth;
      ZoneVector<CRef> refs;

      CLabel(Zone* zone, size_t v)
          : target(nullptr), value_depth(v), refs(zone) {}

      // Bind this label to the given PC.
      void Bind(ControlTransferMap* map, const byte* start, const byte* pc,
                bool expect_value) {
        DCHECK_NULL(target);
        target = pc;
        for (auto from : refs) {
          auto pcdiff = static_cast<pcdiff_t>(target - from.pc);
          auto spdiff = static_cast<spdiff_t>(from.value_depth - value_depth);
          ControlTransfer::StackAction action = ControlTransfer::kNoAction;
          if (expect_value && !from.explicit_value) {
            action = spdiff == 0 ? ControlTransfer::kPushVoid
                                 : ControlTransfer::kPopAndRepush;
          }
          pc_t offset = static_cast<size_t>(from.pc - start);
          (*map)[offset] = {pcdiff, spdiff, action};
        }
      }

      // Reference this label from the given location.
      void Ref(ControlTransferMap* map, const byte* start, CRef from) {
        DCHECK_GE(from.value_depth, value_depth);
        if (target) {
          auto pcdiff = static_cast<pcdiff_t>(target - from.pc);
          auto spdiff = static_cast<spdiff_t>(from.value_depth - value_depth);
          pc_t offset = static_cast<size_t>(from.pc - start);
          (*map)[offset] = {pcdiff, spdiff, ControlTransfer::kNoAction};
        } else {
          refs.push_back(from);
        }
      }
    };

    // An entry in the control stack.
    struct Control {
      const byte* pc;
      CLabel* end_label;
      CLabel* else_label;

      void Ref(ControlTransferMap* map, const byte* start, const byte* from_pc,
               size_t from_value_depth, bool explicit_value) {
        end_label->Ref(map, start, {from_pc, from_value_depth, explicit_value});
      }
    };

    // Compute the ControlTransfer map.
    // This works by maintaining a stack of control constructs similar to the
    // AST decoder. The {control_stack} allows matching {br,br_if,br_table}
    // bytecodes with their target, as well as determining whether the current
    // bytecodes are within the true or false block of an else.
    // The value stack depth is tracked as {value_depth} and is needed to
    // determine how many values to pop off the stack for explicit and
    // implicit control flow.

    std::vector<Control> control_stack;
    size_t value_depth = 0;
    Decoder decoder(start, end);  // for reading operands.
    const byte* pc = start + locals_encoded_size;

    while (pc < end) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      TRACE("@%td: control %s (depth = %zu)\n", (pc - start),
            WasmOpcodes::OpcodeName(opcode), value_depth);
      switch (opcode) {
        case kExprBlock: {
          TRACE("control @%td $%zu: Block\n", (pc - start), value_depth);
          CLabel* label = new (zone) CLabel(zone, value_depth);
          control_stack.push_back({pc, label, nullptr});
          break;
        }
        case kExprLoop: {
          TRACE("control @%td $%zu: Loop\n", (pc - start), value_depth);
          CLabel* label1 = new (zone) CLabel(zone, value_depth);
          CLabel* label2 = new (zone) CLabel(zone, value_depth);
          control_stack.push_back({pc, label1, nullptr});
          control_stack.push_back({pc, label2, nullptr});
          label2->Bind(&map_, start, pc, false);
          break;
        }
        case kExprIf: {
          TRACE("control @%td $%zu: If\n", (pc - start), value_depth);
          value_depth--;
          CLabel* end_label = new (zone) CLabel(zone, value_depth);
          CLabel* else_label = new (zone) CLabel(zone, value_depth);
          control_stack.push_back({pc, end_label, else_label});
          else_label->Ref(&map_, start, {pc, value_depth, false});
          break;
        }
        case kExprElse: {
          Control* c = &control_stack.back();
          TRACE("control @%td $%zu: Else\n", (pc - start), value_depth);
          c->end_label->Ref(&map_, start, {pc, value_depth, false});
          value_depth = c->end_label->value_depth;
          DCHECK_NOT_NULL(c->else_label);
          c->else_label->Bind(&map_, start, pc + 1, false);
          c->else_label = nullptr;
          break;
        }
        case kExprEnd: {
          Control* c = &control_stack.back();
          TRACE("control @%td $%zu: End\n", (pc - start), value_depth);
          if (c->end_label->target) {
            // only loops have bound labels.
            DCHECK_EQ(kExprLoop, *c->pc);
            control_stack.pop_back();
            c = &control_stack.back();
          }
          if (c->else_label) c->else_label->Bind(&map_, start, pc + 1, true);
          c->end_label->Ref(&map_, start, {pc, value_depth, false});
          c->end_label->Bind(&map_, start, pc + 1, true);
          value_depth = c->end_label->value_depth + 1;
          control_stack.pop_back();
          break;
        }
        case kExprBr: {
          BreakDepthOperand operand(&decoder, pc);
          TRACE("control @%td $%zu: Br[arity=%u, depth=%u]\n", (pc - start),
                value_depth, operand.arity, operand.depth);
          value_depth -= operand.arity;
          control_stack[control_stack.size() - operand.depth - 1].Ref(
              &map_, start, pc, value_depth, operand.arity > 0);
          value_depth++;
          break;
        }
        case kExprBrIf: {
          BreakDepthOperand operand(&decoder, pc);
          TRACE("control @%td $%zu: BrIf[arity=%u, depth=%u]\n", (pc - start),
                value_depth, operand.arity, operand.depth);
          value_depth -= (operand.arity + 1);
          control_stack[control_stack.size() - operand.depth - 1].Ref(
              &map_, start, pc, value_depth, operand.arity > 0);
          value_depth++;
          break;
        }
        case kExprBrTable: {
          BranchTableOperand operand(&decoder, pc);
          TRACE("control @%td $%zu: BrTable[arity=%u count=%u]\n", (pc - start),
                value_depth, operand.arity, operand.table_count);
          value_depth -= (operand.arity + 1);
          for (uint32_t i = 0; i < operand.table_count + 1; ++i) {
            uint32_t target = operand.read_entry(&decoder, i);
            control_stack[control_stack.size() - target - 1].Ref(
                &map_, start, pc + i, value_depth, operand.arity > 0);
          }
          value_depth++;
          break;
        }
        default: {
          value_depth = value_depth - OpcodeArity(pc, end) + 1;
          break;
        }
      }

      pc += OpcodeLength(pc, end);
    }
  }

  ControlTransfer Lookup(pc_t from) {
    auto result = map_.find(from);
    if (result == map_.end()) {
      V8_Fatal(__FILE__, __LINE__, "no control target for pc %zu", from);
    }
    return result->second;
  }
};

// Code and metadata needed to execute a function.
struct InterpreterCode {
  const WasmFunction* function;  // wasm function
  AstLocalDecls locals;          // local declarations
  const byte* orig_start;        // start of original code
  const byte* orig_end;          // end of original code
  byte* start;                   // start of (maybe altered) code
  byte* end;                     // end of (maybe altered) code
  ControlTransfers* targets;     // helper for control flow.

  const byte* at(pc_t pc) { return start + pc; }
};

// The main storage for interpreter code. It maps {WasmFunction} to the
// metadata needed to execute each function.
class CodeMap {
 public:
  Zone* zone_;
  const WasmModule* module_;
  ZoneVector<InterpreterCode> interpreter_code_;

  CodeMap(const WasmModule* module, Zone* zone)
      : zone_(zone), module_(module), interpreter_code_(zone) {
    if (module == nullptr) return;
    for (size_t i = 0; i < module->functions.size(); ++i) {
      const WasmFunction* function = &module->functions[i];
      const byte* code_start =
          module->module_start + function->code_start_offset;
      const byte* code_end = module->module_start + function->code_end_offset;
      AddFunction(function, code_start, code_end);
    }
  }

  InterpreterCode* FindCode(const WasmFunction* function) {
    if (function->func_index < interpreter_code_.size()) {
      InterpreterCode* code = &interpreter_code_[function->func_index];
      DCHECK_EQ(function, code->function);
      return code;
    }
    return nullptr;
  }

  InterpreterCode* GetCode(uint32_t function_index) {
    CHECK_LT(function_index, interpreter_code_.size());
    return Preprocess(&interpreter_code_[function_index]);
  }

  InterpreterCode* GetIndirectCode(uint32_t indirect_index) {
    if (indirect_index >= module_->function_table.size()) return nullptr;
    uint32_t index = module_->function_table[indirect_index];
    if (index >= interpreter_code_.size()) return nullptr;
    return GetCode(index);
  }

  InterpreterCode* Preprocess(InterpreterCode* code) {
    if (code->targets == nullptr && code->start) {
      // Compute the control targets map and the local declarations.
      CHECK(DecodeLocalDecls(code->locals, code->start, code->end));
      code->targets =
          new (zone_) ControlTransfers(zone_, code->locals.decls_encoded_size,
                                       code->orig_start, code->orig_end);
    }
    return code;
  }

  int AddFunction(const WasmFunction* function, const byte* code_start,
                  const byte* code_end) {
    InterpreterCode code = {
        function, AstLocalDecls(zone_),          code_start,
        code_end, const_cast<byte*>(code_start), const_cast<byte*>(code_end),
        nullptr};

    DCHECK_EQ(interpreter_code_.size(), function->func_index);
    interpreter_code_.push_back(code);
    return static_cast<int>(interpreter_code_.size()) - 1;
  }

  bool SetFunctionCode(const WasmFunction* function, const byte* start,
                       const byte* end) {
    InterpreterCode* code = FindCode(function);
    if (code == nullptr) return false;
    code->targets = nullptr;
    code->orig_start = start;
    code->orig_end = end;
    code->start = const_cast<byte*>(start);
    code->end = const_cast<byte*>(end);
    Preprocess(code);
    return true;
  }
};

// Responsible for executing code directly.
class ThreadImpl : public WasmInterpreter::Thread {
 public:
  ThreadImpl(Zone* zone, CodeMap* codemap, WasmModuleInstance* instance)
      : codemap_(codemap),
        instance_(instance),
        stack_(zone),
        frames_(zone),
        state_(WasmInterpreter::STOPPED),
        break_pc_(kInvalidPc),
        trap_reason_(kTrapCount) {}

  virtual ~ThreadImpl() {}

  //==========================================================================
  // Implementation of public interface for WasmInterpreter::Thread.
  //==========================================================================

  virtual WasmInterpreter::State state() { return state_; }

  virtual void PushFrame(const WasmFunction* function, WasmVal* args) {
    InterpreterCode* code = codemap()->FindCode(function);
    CHECK_NOT_NULL(code);
    frames_.push_back({code, 0, 0, stack_.size()});
    for (size_t i = 0; i < function->sig->parameter_count(); ++i) {
      stack_.push_back(args[i]);
    }
    frames_.back().ret_pc = InitLocals(code);
    TRACE("  => PushFrame(#%u @%zu)\n", code->function->func_index,
          frames_.back().ret_pc);
  }

  virtual WasmInterpreter::State Run() {
    do {
      TRACE("  => Run()\n");
      if (state_ == WasmInterpreter::STOPPED ||
          state_ == WasmInterpreter::PAUSED) {
        state_ = WasmInterpreter::RUNNING;
        Execute(frames_.back().code, frames_.back().ret_pc, kRunSteps);
      }
    } while (state_ == WasmInterpreter::STOPPED);
    return state_;
  }

  virtual WasmInterpreter::State Step() {
    TRACE("  => Step()\n");
    if (state_ == WasmInterpreter::STOPPED ||
        state_ == WasmInterpreter::PAUSED) {
      state_ = WasmInterpreter::RUNNING;
      Execute(frames_.back().code, frames_.back().ret_pc, 1);
    }
    return state_;
  }

  virtual void Pause() { UNIMPLEMENTED(); }

  virtual void Reset() {
    TRACE("----- RESET -----\n");
    stack_.clear();
    frames_.clear();
    state_ = WasmInterpreter::STOPPED;
    trap_reason_ = kTrapCount;
  }

  virtual int GetFrameCount() { return static_cast<int>(frames_.size()); }

  virtual const WasmFrame* GetFrame(int index) {
    UNIMPLEMENTED();
    return nullptr;
  }

  virtual WasmFrame* GetMutableFrame(int index) {
    UNIMPLEMENTED();
    return nullptr;
  }

  virtual WasmVal GetReturnValue() {
    if (state_ == WasmInterpreter::TRAPPED) return WasmVal(0xdeadbeef);
    CHECK_EQ(WasmInterpreter::FINISHED, state_);
    CHECK_EQ(1, stack_.size());
    return stack_[0];
  }

  virtual pc_t GetBreakpointPc() { return break_pc_; }

  bool Terminated() {
    return state_ == WasmInterpreter::TRAPPED ||
           state_ == WasmInterpreter::FINISHED;
  }

 private:
  // Entries on the stack of functions being evaluated.
  struct Frame {
    InterpreterCode* code;
    pc_t call_pc;
    pc_t ret_pc;
    sp_t sp;

    // Limit of parameters.
    sp_t plimit() { return sp + code->function->sig->parameter_count(); }
    // Limit of locals.
    sp_t llimit() { return plimit() + code->locals.total_local_count; }
  };

  CodeMap* codemap_;
  WasmModuleInstance* instance_;
  ZoneVector<WasmVal> stack_;
  ZoneVector<Frame> frames_;
  WasmInterpreter::State state_;
  pc_t break_pc_;
  TrapReason trap_reason_;

  CodeMap* codemap() { return codemap_; }
  WasmModuleInstance* instance() { return instance_; }
  const WasmModule* module() { return instance_->module; }

  void DoTrap(TrapReason trap, pc_t pc) {
    state_ = WasmInterpreter::TRAPPED;
    trap_reason_ = trap;
    CommitPc(pc);
  }

  // Push a frame with arguments already on the stack.
  void PushFrame(InterpreterCode* code, pc_t call_pc, pc_t ret_pc) {
    CHECK_NOT_NULL(code);
    DCHECK(!frames_.empty());
    frames_.back().call_pc = call_pc;
    frames_.back().ret_pc = ret_pc;
    size_t arity = code->function->sig->parameter_count();
    DCHECK_GE(stack_.size(), arity);
    // The parameters will overlap the arguments already on the stack.
    frames_.push_back({code, 0, 0, stack_.size() - arity});
    frames_.back().ret_pc = InitLocals(code);
    TRACE("  => push func#%u @%zu\n", code->function->func_index,
          frames_.back().ret_pc);
  }

  pc_t InitLocals(InterpreterCode* code) {
    for (auto p : code->locals.local_types) {
      WasmVal val;
      switch (p.first) {
        case kAstI32:
          val = WasmVal(static_cast<int32_t>(0));
          break;
        case kAstI64:
          val = WasmVal(static_cast<int64_t>(0));
          break;
        case kAstF32:
          val = WasmVal(static_cast<float>(0));
          break;
        case kAstF64:
          val = WasmVal(static_cast<double>(0));
          break;
        default:
          UNREACHABLE();
          break;
      }
      stack_.insert(stack_.end(), p.second, val);
    }
    return code->locals.decls_encoded_size;
  }

  void CommitPc(pc_t pc) {
    if (!frames_.empty()) {
      frames_.back().ret_pc = pc;
    }
  }

  bool SkipBreakpoint(InterpreterCode* code, pc_t pc) {
    if (pc == break_pc_) {
      break_pc_ = kInvalidPc;
      return true;
    }
    return false;
  }

  bool DoReturn(InterpreterCode** code, pc_t* pc, pc_t* limit, WasmVal val) {
    DCHECK_GT(frames_.size(), 0u);
    stack_.resize(frames_.back().sp);
    frames_.pop_back();
    if (frames_.size() == 0) {
      // A return from the top frame terminates the execution.
      state_ = WasmInterpreter::FINISHED;
      stack_.clear();
      stack_.push_back(val);
      TRACE("  => finish\n");
      return false;
    } else {
      // Return to caller frame.
      Frame* top = &frames_.back();
      *code = top->code;
      *pc = top->ret_pc;
      *limit = top->code->end - top->code->start;
      if (top->code->start[top->call_pc] == kExprCallIndirect ||
          (top->code->orig_start &&
           top->code->orig_start[top->call_pc] == kExprCallIndirect)) {
        // UGLY: An indirect call has the additional function index on the
        // stack.
        stack_.pop_back();
      }
      TRACE("  => pop func#%u @%zu\n", (*code)->function->func_index, *pc);

      stack_.push_back(val);
      return true;
    }
  }

  void DoCall(InterpreterCode* target, pc_t* pc, pc_t ret_pc, pc_t* limit) {
    PushFrame(target, *pc, ret_pc);
    *pc = frames_.back().ret_pc;
    *limit = target->end - target->start;
  }

  // Adjust the program counter {pc} and the stack contents according to the
  // code's precomputed control transfer map. Returns the different between
  // the new pc and the old pc.
  int DoControlTransfer(InterpreterCode* code, pc_t pc) {
    auto target = code->targets->Lookup(pc);
    switch (target.action) {
      case ControlTransfer::kNoAction:
        TRACE("  action [sp-%u]\n", target.spdiff);
        PopN(target.spdiff);
        break;
      case ControlTransfer::kPopAndRepush: {
        WasmVal val = Pop();
        TRACE("  action [pop x, sp-%u, push x]\n", target.spdiff - 1);
        DCHECK_GE(target.spdiff, 1u);
        PopN(target.spdiff - 1);
        Push(pc, val);
        break;
      }
      case ControlTransfer::kPushVoid:
        TRACE("  action [sp-%u, push void]\n", target.spdiff);
        PopN(target.spdiff);
        Push(pc, WasmVal());
        break;
    }
    return target.pcdiff;
  }

  void Execute(InterpreterCode* code, pc_t pc, int max) {
    Decoder decoder(code->start, code->end);
    pc_t limit = code->end - code->start;
    while (true) {
      if (max-- <= 0) {
        // Maximum number of instructions reached.
        state_ = WasmInterpreter::PAUSED;
        return CommitPc(pc);
      }

      if (pc >= limit) {
        // Fell off end of code; do an implicit return.
        TRACE("@%-3zu: ImplicitReturn\n", pc);
        WasmVal val = PopArity(code->function->sig->return_count());
        if (!DoReturn(&code, &pc, &limit, val)) return;
        decoder.Reset(code->start, code->end);
        continue;
      }

      const char* skip = "        ";
      int len = 1;
      byte opcode = code->start[pc];
      byte orig = opcode;
      if (opcode == kInternalBreakpoint) {
        orig = code->orig_start[pc];
        if (SkipBreakpoint(code, pc)) {
          // skip breakpoint by switching on original code.
          skip = "[skip]  ";
        } else {
          state_ = WasmInterpreter::PAUSED;
          TRACE("@%-3zu: [break] %-24s:", pc,
                WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(orig)));
          TraceValueStack();
          TRACE("\n");
          break_pc_ = pc;
          return CommitPc(pc);
        }
      }

      USE(skip);
      TRACE("@%-3zu: %s%-24s:", pc, skip,
            WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(orig)));
      TraceValueStack();
      TRACE("\n");

      switch (orig) {
        case kExprNop:
          Push(pc, WasmVal());
          break;
        case kExprBlock:
        case kExprLoop: {
          // Do nothing.
          break;
        }
        case kExprIf: {
          WasmVal cond = Pop();
          bool is_true = cond.to<uint32_t>() != 0;
          if (is_true) {
            // fall through to the true block.
            TRACE("  true => fallthrough\n");
          } else {
            len = DoControlTransfer(code, pc);
            TRACE("  false => @%zu\n", pc + len);
          }
          break;
        }
        case kExprElse: {
          len = DoControlTransfer(code, pc);
          TRACE("  end => @%zu\n", pc + len);
          break;
        }
        case kExprSelect: {
          WasmVal cond = Pop();
          WasmVal fval = Pop();
          WasmVal tval = Pop();
          Push(pc, cond.to<int32_t>() != 0 ? tval : fval);
          break;
        }
        case kExprBr: {
          BreakDepthOperand operand(&decoder, code->at(pc));
          WasmVal val = PopArity(operand.arity);
          len = DoControlTransfer(code, pc);
          TRACE("  br => @%zu\n", pc + len);
          if (operand.arity > 0) Push(pc, val);
          break;
        }
        case kExprBrIf: {
          BreakDepthOperand operand(&decoder, code->at(pc));
          WasmVal cond = Pop();
          WasmVal val = PopArity(operand.arity);
          bool is_true = cond.to<uint32_t>() != 0;
          if (is_true) {
            len = DoControlTransfer(code, pc);
            TRACE("  br_if => @%zu\n", pc + len);
            if (operand.arity > 0) Push(pc, val);
          } else {
            TRACE("  false => fallthrough\n");
            len = 1 + operand.length;
            Push(pc, WasmVal());
          }
          break;
        }
        case kExprBrTable: {
          BranchTableOperand operand(&decoder, code->at(pc));
          uint32_t key = Pop().to<uint32_t>();
          WasmVal val = PopArity(operand.arity);
          if (key >= operand.table_count) key = operand.table_count;
          len = DoControlTransfer(code, pc + key) + key;
          TRACE("  br[%u] => @%zu\n", key, pc + len);
          if (operand.arity > 0) Push(pc, val);
          break;
        }
        case kExprReturn: {
          ReturnArityOperand operand(&decoder, code->at(pc));
          WasmVal val = PopArity(operand.arity);
          if (!DoReturn(&code, &pc, &limit, val)) return;
          decoder.Reset(code->start, code->end);
          continue;
        }
        case kExprUnreachable: {
          DoTrap(kTrapUnreachable, pc);
          return CommitPc(pc);
        }
        case kExprEnd: {
          len = DoControlTransfer(code, pc);
          DCHECK_EQ(1, len);
          break;
        }
        case kExprI8Const: {
          ImmI8Operand operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprI32Const: {
          ImmI32Operand operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Operand operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Operand operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Operand operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprGetLocal: {
          LocalIndexOperand operand(&decoder, code->at(pc));
          Push(pc, stack_[frames_.back().sp + operand.index]);
          len = 1 + operand.length;
          break;
        }
        case kExprSetLocal: {
          LocalIndexOperand operand(&decoder, code->at(pc));
          WasmVal val = Pop();
          stack_[frames_.back().sp + operand.index] = val;
          Push(pc, val);
          len = 1 + operand.length;
          break;
        }
        case kExprCallFunction: {
          CallFunctionOperand operand(&decoder, code->at(pc));
          InterpreterCode* target = codemap()->GetCode(operand.index);
          DoCall(target, &pc, pc + 1 + operand.length, &limit);
          code = target;
          decoder.Reset(code->start, code->end);
          continue;
        }
        case kExprCallIndirect: {
          CallIndirectOperand operand(&decoder, code->at(pc));
          size_t index = stack_.size() - operand.arity - 1;
          DCHECK_LT(index, stack_.size());
          uint32_t table_index = stack_[index].to<uint32_t>();
          if (table_index >= module()->function_table.size()) {
            return DoTrap(kTrapFuncInvalid, pc);
          }
          uint16_t function_index = module()->function_table[table_index];
          InterpreterCode* target = codemap()->GetCode(function_index);
          DCHECK(target);
          if (target->function->sig_index != operand.index) {
            return DoTrap(kTrapFuncSigMismatch, pc);
          }

          DoCall(target, &pc, pc + 1 + operand.length, &limit);
          code = target;
          decoder.Reset(code->start, code->end);
          continue;
        }
        case kExprCallImport: {
          UNIMPLEMENTED();
          break;
        }
        case kExprLoadGlobal: {
          GlobalIndexOperand operand(&decoder, code->at(pc));
          const WasmGlobal* global = &module()->globals[operand.index];
          byte* ptr = instance()->globals_start + global->offset;
          MachineType type = global->type;
          WasmVal val;
          if (type == MachineType::Int8()) {
            val =
                WasmVal(static_cast<int32_t>(*reinterpret_cast<int8_t*>(ptr)));
          } else if (type == MachineType::Uint8()) {
            val =
                WasmVal(static_cast<int32_t>(*reinterpret_cast<uint8_t*>(ptr)));
          } else if (type == MachineType::Int16()) {
            val =
                WasmVal(static_cast<int32_t>(*reinterpret_cast<int16_t*>(ptr)));
          } else if (type == MachineType::Uint16()) {
            val = WasmVal(
                static_cast<int32_t>(*reinterpret_cast<uint16_t*>(ptr)));
          } else if (type == MachineType::Int32()) {
            val = WasmVal(*reinterpret_cast<int32_t*>(ptr));
          } else if (type == MachineType::Uint32()) {
            val = WasmVal(*reinterpret_cast<uint32_t*>(ptr));
          } else if (type == MachineType::Int64()) {
            val = WasmVal(*reinterpret_cast<int64_t*>(ptr));
          } else if (type == MachineType::Uint64()) {
            val = WasmVal(*reinterpret_cast<uint64_t*>(ptr));
          } else if (type == MachineType::Float32()) {
            val = WasmVal(*reinterpret_cast<float*>(ptr));
          } else if (type == MachineType::Float64()) {
            val = WasmVal(*reinterpret_cast<double*>(ptr));
          } else {
            UNREACHABLE();
          }
          Push(pc, val);
          len = 1 + operand.length;
          break;
        }
        case kExprStoreGlobal: {
          GlobalIndexOperand operand(&decoder, code->at(pc));
          const WasmGlobal* global = &module()->globals[operand.index];
          byte* ptr = instance()->globals_start + global->offset;
          MachineType type = global->type;
          WasmVal val = Pop();
          if (type == MachineType::Int8()) {
            *reinterpret_cast<int8_t*>(ptr) =
                static_cast<int8_t>(val.to<int32_t>());
          } else if (type == MachineType::Uint8()) {
            *reinterpret_cast<uint8_t*>(ptr) =
                static_cast<uint8_t>(val.to<uint32_t>());
          } else if (type == MachineType::Int16()) {
            *reinterpret_cast<int16_t*>(ptr) =
                static_cast<int16_t>(val.to<int32_t>());
          } else if (type == MachineType::Uint16()) {
            *reinterpret_cast<uint16_t*>(ptr) =
                static_cast<uint16_t>(val.to<uint32_t>());
          } else if (type == MachineType::Int32()) {
            *reinterpret_cast<int32_t*>(ptr) = val.to<int32_t>();
          } else if (type == MachineType::Uint32()) {
            *reinterpret_cast<uint32_t*>(ptr) = val.to<uint32_t>();
          } else if (type == MachineType::Int64()) {
            *reinterpret_cast<int64_t*>(ptr) = val.to<int64_t>();
          } else if (type == MachineType::Uint64()) {
            *reinterpret_cast<uint64_t*>(ptr) = val.to<uint64_t>();
          } else if (type == MachineType::Float32()) {
            *reinterpret_cast<float*>(ptr) = val.to<float>();
          } else if (type == MachineType::Float64()) {
            *reinterpret_cast<double*>(ptr) = val.to<double>();
          } else {
            UNREACHABLE();
          }
          Push(pc, val);
          len = 1 + operand.length;
          break;
        }

#define LOAD_CASE(name, ctype, mtype)                                       \
  case kExpr##name: {                                                       \
    MemoryAccessOperand operand(&decoder, code->at(pc));                    \
    uint32_t index = Pop().to<uint32_t>();                                  \
    size_t effective_mem_size = instance()->mem_size - sizeof(mtype);       \
    if (operand.offset > effective_mem_size ||                              \
        index > (effective_mem_size - operand.offset)) {                    \
      return DoTrap(kTrapMemOutOfBounds, pc);                               \
    }                                                                       \
    byte* addr = instance()->mem_start + operand.offset + index;            \
    WasmVal result(static_cast<ctype>(ReadLittleEndianValue<mtype>(addr))); \
    Push(pc, result);                                                       \
    len = 1 + operand.length;                                               \
    break;                                                                  \
  }

          LOAD_CASE(I32LoadMem8S, int32_t, int8_t);
          LOAD_CASE(I32LoadMem8U, int32_t, uint8_t);
          LOAD_CASE(I32LoadMem16S, int32_t, int16_t);
          LOAD_CASE(I32LoadMem16U, int32_t, uint16_t);
          LOAD_CASE(I64LoadMem8S, int64_t, int8_t);
          LOAD_CASE(I64LoadMem8U, int64_t, uint8_t);
          LOAD_CASE(I64LoadMem16S, int64_t, int16_t);
          LOAD_CASE(I64LoadMem16U, int64_t, uint16_t);
          LOAD_CASE(I64LoadMem32S, int64_t, int32_t);
          LOAD_CASE(I64LoadMem32U, int64_t, uint32_t);
          LOAD_CASE(I32LoadMem, int32_t, int32_t);
          LOAD_CASE(I64LoadMem, int64_t, int64_t);
          LOAD_CASE(F32LoadMem, float, float);
          LOAD_CASE(F64LoadMem, double, double);
#undef LOAD_CASE

#define STORE_CASE(name, ctype, mtype)                                        \
  case kExpr##name: {                                                         \
    MemoryAccessOperand operand(&decoder, code->at(pc));                      \
    WasmVal val = Pop();                                                      \
    uint32_t index = Pop().to<uint32_t>();                                    \
    size_t effective_mem_size = instance()->mem_size - sizeof(mtype);         \
    if (operand.offset > effective_mem_size ||                                \
        index > (effective_mem_size - operand.offset)) {                      \
      return DoTrap(kTrapMemOutOfBounds, pc);                                 \
    }                                                                         \
    byte* addr = instance()->mem_start + operand.offset + index;              \
    WriteLittleEndianValue<mtype>(addr, static_cast<mtype>(val.to<ctype>())); \
    Push(pc, val);                                                            \
    len = 1 + operand.length;                                                 \
    break;                                                                    \
  }

          STORE_CASE(I32StoreMem8, int32_t, int8_t);
          STORE_CASE(I32StoreMem16, int32_t, int16_t);
          STORE_CASE(I64StoreMem8, int64_t, int8_t);
          STORE_CASE(I64StoreMem16, int64_t, int16_t);
          STORE_CASE(I64StoreMem32, int64_t, int32_t);
          STORE_CASE(I32StoreMem, int32_t, int32_t);
          STORE_CASE(I64StoreMem, int64_t, int64_t);
          STORE_CASE(F32StoreMem, float, float);
          STORE_CASE(F64StoreMem, double, double);
#undef STORE_CASE

#define ASMJS_LOAD_CASE(name, ctype, mtype, defval)                 \
  case kExpr##name: {                                               \
    uint32_t index = Pop().to<uint32_t>();                          \
    ctype result;                                                   \
    if (index >= (instance()->mem_size - sizeof(mtype))) {          \
      result = defval;                                              \
    } else {                                                        \
      byte* addr = instance()->mem_start + index;                   \
      /* TODO(titzer): alignment for asmjs load mem? */             \
      result = static_cast<ctype>(*reinterpret_cast<mtype*>(addr)); \
    }                                                               \
    Push(pc, WasmVal(result));                                      \
    break;                                                          \
  }
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8S, int32_t, int8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8U, int32_t, uint8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16S, int32_t, int16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16U, int32_t, uint16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem, int32_t, int32_t, 0);
          ASMJS_LOAD_CASE(F32AsmjsLoadMem, float, float,
                          std::numeric_limits<float>::quiet_NaN());
          ASMJS_LOAD_CASE(F64AsmjsLoadMem, double, double,
                          std::numeric_limits<double>::quiet_NaN());
#undef ASMJS_LOAD_CASE

#define ASMJS_STORE_CASE(name, ctype, mtype)                                   \
  case kExpr##name: {                                                          \
    WasmVal val = Pop();                                                       \
    uint32_t index = Pop().to<uint32_t>();                                     \
    if (index < (instance()->mem_size - sizeof(mtype))) {                      \
      byte* addr = instance()->mem_start + index;                              \
      /* TODO(titzer): alignment for asmjs store mem? */                       \
      *(reinterpret_cast<mtype*>(addr)) = static_cast<mtype>(val.to<ctype>()); \
    }                                                                          \
    Push(pc, val);                                                             \
    break;                                                                     \
  }

          ASMJS_STORE_CASE(I32AsmjsStoreMem8, int32_t, int8_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem16, int32_t, int16_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem, int32_t, int32_t);
          ASMJS_STORE_CASE(F32AsmjsStoreMem, float, float);
          ASMJS_STORE_CASE(F64AsmjsStoreMem, double, double);
#undef ASMJS_STORE_CASE

        case kExprMemorySize: {
          Push(pc, WasmVal(static_cast<uint32_t>(instance()->mem_size)));
          break;
        }
#define EXECUTE_SIMPLE_BINOP(name, ctype, op)             \
  case kExpr##name: {                                     \
    WasmVal rval = Pop();                                 \
    WasmVal lval = Pop();                                 \
    WasmVal result(lval.to<ctype>() op rval.to<ctype>()); \
    Push(pc, result);                                     \
    break;                                                \
  }
          FOREACH_SIMPLE_BINOP(EXECUTE_SIMPLE_BINOP)
#undef EXECUTE_SIMPLE_BINOP

#define EXECUTE_OTHER_BINOP(name, ctype)              \
  case kExpr##name: {                                 \
    TrapReason trap = kTrapCount;                     \
    volatile ctype rval = Pop().to<ctype>();          \
    volatile ctype lval = Pop().to<ctype>();          \
    WasmVal result(Execute##name(lval, rval, &trap)); \
    if (trap != kTrapCount) return DoTrap(trap, pc);  \
    Push(pc, result);                                 \
    break;                                            \
  }
          FOREACH_OTHER_BINOP(EXECUTE_OTHER_BINOP)
#undef EXECUTE_OTHER_BINOP

#define EXECUTE_OTHER_UNOP(name, ctype)              \
  case kExpr##name: {                                \
    TrapReason trap = kTrapCount;                    \
    volatile ctype val = Pop().to<ctype>();          \
    WasmVal result(Execute##name(val, &trap));       \
    if (trap != kTrapCount) return DoTrap(trap, pc); \
    Push(pc, result);                                \
    break;                                           \
  }
          FOREACH_OTHER_UNOP(EXECUTE_OTHER_UNOP)
#undef EXECUTE_OTHER_UNOP

        default:
          V8_Fatal(__FILE__, __LINE__, "Unknown or unimplemented opcode #%d:%s",
                   code->start[pc], OpcodeName(code->start[pc]));
          UNREACHABLE();
      }

      pc += len;
    }
    UNREACHABLE();  // above decoding loop should run forever.
  }

  WasmVal Pop() {
    DCHECK_GT(stack_.size(), 0u);
    DCHECK_GT(frames_.size(), 0u);
    DCHECK_GT(stack_.size(), frames_.back().llimit());  // can't pop into locals
    WasmVal val = stack_.back();
    stack_.pop_back();
    return val;
  }

  void PopN(int n) {
    DCHECK_GE(stack_.size(), static_cast<size_t>(n));
    DCHECK_GT(frames_.size(), 0u);
    size_t nsize = stack_.size() - n;
    DCHECK_GE(nsize, frames_.back().llimit());  // can't pop into locals
    stack_.resize(nsize);
  }

  WasmVal PopArity(size_t arity) {
    if (arity == 0) return WasmVal();
    CHECK_EQ(1, arity);
    return Pop();
  }

  void Push(pc_t pc, WasmVal val) {
    // TODO(titzer): store PC as well?
    stack_.push_back(val);
  }

  void TraceStack(const char* phase, pc_t pc) {
    if (FLAG_trace_wasm_interpreter) {
      PrintF("%s @%zu", phase, pc);
      UNIMPLEMENTED();
      PrintF("\n");
    }
  }

  void TraceValueStack() {
    Frame* top = frames_.size() > 0 ? &frames_.back() : nullptr;
    sp_t sp = top ? top->sp : 0;
    sp_t plimit = top ? top->plimit() : 0;
    sp_t llimit = top ? top->llimit() : 0;
    if (FLAG_trace_wasm_interpreter) {
      for (size_t i = sp; i < stack_.size(); ++i) {
        if (i < plimit)
          PrintF(" p%zu:", i);
        else if (i < llimit)
          PrintF(" l%zu:", i);
        else
          PrintF(" s%zu:", i);
        WasmVal val = stack_[i];
        switch (val.type) {
          case kAstI32:
            PrintF("i32:%d", val.to<int32_t>());
            break;
          case kAstI64:
            PrintF("i64:%" PRId64 "", val.to<int64_t>());
            break;
          case kAstF32:
            PrintF("f32:%f", val.to<float>());
            break;
          case kAstF64:
            PrintF("f64:%lf", val.to<double>());
            break;
          case kAstStmt:
            PrintF("void");
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
    }
  }
};

//============================================================================
// The implementation details of the interpreter.
//============================================================================
class WasmInterpreterInternals : public ZoneObject {
 public:
  WasmModuleInstance* instance_;
  CodeMap codemap_;
  ZoneVector<ThreadImpl*> threads_;

  WasmInterpreterInternals(Zone* zone, WasmModuleInstance* instance)
      : instance_(instance),
        codemap_(instance_ ? instance_->module : nullptr, zone),
        threads_(zone) {
    threads_.push_back(new ThreadImpl(zone, &codemap_, instance));
  }

  void Delete() {
    // TODO(titzer): CFI doesn't like threads in the ZoneVector.
    for (auto t : threads_) delete t;
    threads_.resize(0);
  }
};

//============================================================================
// Implementation of the public interface of the interpreter.
//============================================================================
WasmInterpreter::WasmInterpreter(WasmModuleInstance* instance,
                                 base::AccountingAllocator* allocator)
    : zone_(allocator),
      internals_(new (&zone_) WasmInterpreterInternals(&zone_, instance)) {}

WasmInterpreter::~WasmInterpreter() { internals_->Delete(); }

void WasmInterpreter::Run() { internals_->threads_[0]->Run(); }

void WasmInterpreter::Pause() { internals_->threads_[0]->Pause(); }

bool WasmInterpreter::SetBreakpoint(const WasmFunction* function, pc_t pc,
                                    bool enabled) {
  InterpreterCode* code = internals_->codemap_.FindCode(function);
  if (!code) return false;
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.decls_encoded_size || pc >= size) return false;
  // Make a copy of the code before enabling a breakpoint.
  if (enabled && code->orig_start == code->start) {
    code->start = reinterpret_cast<byte*>(zone_.New(size));
    memcpy(code->start, code->orig_start, size);
    code->end = code->start + size;
  }
  bool prev = code->start[pc] == kInternalBreakpoint;
  if (enabled) {
    code->start[pc] = kInternalBreakpoint;
  } else {
    code->start[pc] = code->orig_start[pc];
  }
  return prev;
}

bool WasmInterpreter::GetBreakpoint(const WasmFunction* function, pc_t pc) {
  InterpreterCode* code = internals_->codemap_.FindCode(function);
  if (!code) return false;
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.decls_encoded_size || pc >= size) return false;
  // Check if a breakpoint is present at that place in the code.
  return code->start[pc] == kInternalBreakpoint;
}

bool WasmInterpreter::SetTracing(const WasmFunction* function, bool enabled) {
  UNIMPLEMENTED();
  return false;
}

int WasmInterpreter::GetThreadCount() {
  return 1;  // only one thread for now.
}

WasmInterpreter::Thread* WasmInterpreter::GetThread(int id) {
  CHECK_EQ(0, id);  // only one thread for now.
  return internals_->threads_[id];
}

WasmVal WasmInterpreter::GetLocalVal(const WasmFrame* frame, int index) {
  CHECK_GE(index, 0);
  UNIMPLEMENTED();
  WasmVal none;
  none.type = kAstStmt;
  return none;
}

WasmVal WasmInterpreter::GetExprVal(const WasmFrame* frame, int pc) {
  UNIMPLEMENTED();
  WasmVal none;
  none.type = kAstStmt;
  return none;
}

void WasmInterpreter::SetLocalVal(WasmFrame* frame, int index, WasmVal val) {
  UNIMPLEMENTED();
}

void WasmInterpreter::SetExprVal(WasmFrame* frame, int pc, WasmVal val) {
  UNIMPLEMENTED();
}

size_t WasmInterpreter::GetMemorySize() {
  return internals_->instance_->mem_size;
}

WasmVal WasmInterpreter::ReadMemory(size_t offset) {
  UNIMPLEMENTED();
  return WasmVal();
}

void WasmInterpreter::WriteMemory(size_t offset, WasmVal val) {
  UNIMPLEMENTED();
}

int WasmInterpreter::AddFunctionForTesting(const WasmFunction* function) {
  return internals_->codemap_.AddFunction(function, nullptr, nullptr);
}

bool WasmInterpreter::SetFunctionCodeForTesting(const WasmFunction* function,
                                                const byte* start,
                                                const byte* end) {
  return internals_->codemap_.SetFunctionCode(function, start, end);
}

ControlTransferMap WasmInterpreter::ComputeControlTransfersForTesting(
    Zone* zone, const byte* start, const byte* end) {
  ControlTransfers targets(zone, 0, start, end);
  return targets.map_;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
