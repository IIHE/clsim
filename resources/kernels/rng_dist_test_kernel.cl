#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable
//#pragma OPENCL EXTENSION cl_khr_fp16 : enable

// disable dbg_printf for GPU
#define dbg_printf(format, ...)

// enable printf for CPU
//#pragma OPENCL EXTENSION cl_amd_printf : enable
//#define dbg_printf(format, ...) printf(format, ##__VA_ARGS__)

#ifdef USE_NATIVE_MATH
inline float my_divide(float a, float b) {return native_divide(a,b);}
inline float my_recip(float a) {return native_recip(a);}
inline float my_powr(float a, float b) {return native_powr(a,b);}
inline float my_sqrt(float a) {return native_sqrt(a);}
inline float my_cos(float a) {return native_cos(a);}
inline float my_sin(float a) {return native_sin(a);}
inline float my_log(float a) {return native_log(a);}
inline float my_exp(float a) {return native_exp(a);}
#else
inline float my_divide(float a, float b) {return a/b;}
inline float my_recip(float a) {return 1.f/a;}
inline float my_powr(float a, float b) {return powr(a,b);}
inline float my_sqrt(float a) {return sqrt(a);}
inline float my_cos(float a) {return cos(a);}
inline float my_sin(float a) {return sin(a);}
inline float my_log(float a) {return log(a);}
inline float my_exp(float a) {return exp(a);}
#endif

inline float sqr(float a) {return a*a;}


__kernel void testKernel(__write_only __global float* randomNumbers,
                         __global ulong* MWC_RNG_x,
                         __global uint* MWC_RNG_a)
{
    dbg_printf("Start kernel... (work item %u of %u)\n", get_global_id(0), get_global_size(0));

    unsigned int i = get_global_id(0);
    unsigned int global_size = get_global_size(0);

    //download MWC RNG state
    ulong real_rnd_x = MWC_RNG_x[i];
    uint real_rnd_a = MWC_RNG_a[i];
    ulong *rnd_x = &real_rnd_x;
    uint *rnd_a = &real_rnd_a;

    // call the function and generate a random number according to the requested distribution
    randomNumbers[i] = generateRandomNumberAccordingToDistribution(RNG_ARGS_TO_CALL);

    dbg_printf("Stop kernel... (work item %u of %u)\n", i, global_size);
    dbg_printf("Kernel finished.\n");

    //upload MWC RNG state
    MWC_RNG_x[i] = real_rnd_x;
    MWC_RNG_a[i] = real_rnd_a;
}