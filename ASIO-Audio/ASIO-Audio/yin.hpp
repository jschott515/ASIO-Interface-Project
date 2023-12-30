#ifndef YIN_HPP
#define YIN_HPP

#include <atomic>
#include <complex>
#include <fftw3.h>


class Yin
{
public:
	Yin();
	~Yin();

	void compute_yin(void* audioBuffer);  // Step the input buffer and evaluate a new pitch/harmonic rate
	float get_pitch();                    // returns most recent calculated pitch 
	float get_harmonic_rate();            // returns most recent calculated harmonic rate

private:
	// Audio Info -----------------------------------------------------
	int samplerate = 48000;
	int w_step = 128;  // Size of ASIO input buffer
	int w_len = 1024;  // Should be multiple of w_step... must be > tau_max. I.E. for min f0 of 54Hz (A1) + sr 48kHz, w_len >= 888, thus min latency = .0185 seconds
	int num_steps = w_len / w_step;

	// Yin Parameters -------------------------------------------------
	float harmonic_thresh = 0.10;          // Harmonic rate threshold for detecting a pitch
	int f0_max, f0_min, tau_max, tau_min;  // Max/min measurable frequencies and tau max/min values - (tau_max = samplerate / f0_min)

	// Yin Results ----------------------------------------------------
	float pitch = 0.0;
	float harmonic_rate = 1.0;

	// Buffers --------------------------------------------------------
	void** in_buff;    // Sorta a circular buffer, but simple. Used for audio input.
	int head_idx = 0;  // Head for circular buffer. Increment by (head_idx + 1) % num_steps

	float* yin_buff;    // Buffer for most calculations... valid region varies based on when this is used
	std::complex<float>* fft_buff;  // Complex float buffer for fft calculations. Cast to fftw compatible type with  reinterpret_cast<fftwf_complex*>(fft_buff)
	float* cumsum_buff; // Buffer for cumulative sum calculation during difference function step

	// FFT Parameters -------------------------------------------------
	int size_pad;         // Size of fft kernel
	fftwf_plan fft_plan;  // plan for fft
	fftwf_plan ifft_plan; // plan for ifft


	// Class Methods --------------------------------------------------
	void load_buffer(void* audioBuffer);              // Load step size of samples into input buffer as floats

	void load_yin_buff();            // Load yin_buff from in_buff formated for the difference function. Fills w_len samples from in_buff and zero-pads the rest.
	void calculate_cumsum_for_df();  // Cumulative sum function for difference function. x_cumsum = np.concatenate((np.array([0.]), (x * x).cumsum())). Result is w_len + 1 valid samples in cumsum_buff
	void difference_function();      // Exectue the difference function stage. Result is tau_max # of valid samples in yin_buff. WARNING - The remainder of the buffer is garbage.

	void cumulative_mean_normalized_difference_function();  // Does some math on the result of the difference function. Produces tau_max # of valid samples in yin_buff
	void evaluate_pitch();           // Uses result of cmndf in yin_buff to determine the pitch and harmonic rate
};


#endif
