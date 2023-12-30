#include <string>
#include "yin.hpp"


#define CONV_FACTOR 2147483648.49999


Yin::Yin()
{
	f0_max = 1320; // ~e6 frq.
	f0_min = 54;   // ~a1 frq.
	tau_max = int(samplerate / f0_min);
	tau_min = int(samplerate / f0_max);

	// assert tau_max < w_len
	// assert w_len % w_step == 0

	// assumes sizeof(float) == sizeof(int). a tad sketchy
	in_buff = (void**)new float*[num_steps];
	for (int i = 0; i < num_steps; i++)
	{
		in_buff[i] = (void*)new float[w_step]();
	}

	// optimal kernel size for the difference function fft
	int size = (w_len + tau_max);
	int p2 = floor(log2(size / 32)) + 1;
	int nice_numbers[8] = { 16, 18, 20, 24, 25, 27, 30, 32 };
	for (int i = 0; i < 8; i++)
	{
		size_pad = nice_numbers[i] * pow(2, p2);
		if (size_pad >= size)
			break;
	}
	// should probably assert size_pad > size here?

	yin_buff = new float[size_pad];
	fft_buff = new std::complex<float>[size_pad];
	cumsum_buff = new float[w_len + 1];

	fft_plan = fftwf_plan_dft_r2c_1d(size_pad, yin_buff, reinterpret_cast<fftwf_complex*>(fft_buff), FFTW_ESTIMATE);
	ifft_plan = fftwf_plan_dft_c2r_1d(size_pad, reinterpret_cast<fftwf_complex*>(fft_buff), yin_buff, FFTW_ESTIMATE);
}


Yin::~Yin()
{
	for (int i = 0; i < w_len / w_step; i++)
	{
		delete(in_buff[i]);
	}
	delete(in_buff);
	delete(yin_buff);
	delete(fft_buff);
	delete(cumsum_buff);

	fftwf_destroy_plan(fft_plan);
	fftwf_destroy_plan(ifft_plan);
	fftwf_cleanup();
}


void Yin::load_buffer(void* audioBuffer)
{
	// load data to buffer as int
	memcpy(in_buff[head_idx], audioBuffer, w_step * sizeof(int));
	// convert new data to float
	for (int i = 0; i < w_step; i++)
	{
		((float*)in_buff[head_idx])[i] = (float)((int*)in_buff[head_idx])[i] / CONV_FACTOR;
	}
	head_idx = (head_idx + 1) % num_steps;
}


void Yin::load_yin_buff()
{
	// Format circular buffer as contiguous array for fft
	for (int i = 0; i < num_steps; i++)
	{
		memcpy(&yin_buff[i * w_step], in_buff[(head_idx + i) % num_steps], sizeof(float) * w_step);
	}
	// zero-padding
	memset(&yin_buff[w_len], 0, sizeof(float) * (size_pad - w_len));
}


void Yin::calculate_cumsum_for_df()
{
	// Basically x_cumsum = np.concatenate((np.array([0.]), (x * x).cumsum())) from yin.py
	cumsum_buff[0] = 0.0;
	float sum = 0;
	for (int i = 0; i < w_len; i++)
	{
		float x = yin_buff[i];
		sum += x * x;
		cumsum_buff[i + 1] = sum;
	}
}


void Yin::difference_function()
{
	// assert w_len > tau_max

	// format data from circular buffer to yin_buff array
	load_yin_buff();
	// put cumsum of yin_buff^2 results in cumsum_buff
	calculate_cumsum_for_df();
	// do a fft
	fftwf_execute(fft_plan);
	for (int i = 0; i < size_pad; i++)
	{
		// element-wise multiply conjugate
		fft_buff[i] *= std::conj(fft_buff[i]);
	}
	// back to time domain. note results are scaled by N
	fftwf_execute(ifft_plan);

	for (int i = 0; i < tau_max; i++)
	{
		// Do some math. Stolen from yin.py  - note added (yin_buff / size_pad) to scale the result of the ifft.
		yin_buff[i] = cumsum_buff[w_len - i] + cumsum_buff[w_len] - cumsum_buff[i] - 2 * yin_buff[i] / size_pad;
	}
}


void Yin::cumulative_mean_normalized_difference_function()
{
	// Do some math. Stolen from yin.py - basically cmndf = df[1:] * range(1, N) / np.cumsum(df[1:]).astype(float)
	float sum = 0;
	for (int i = 1; i < tau_max; i++)
	{
		sum += yin_buff[i];
		yin_buff[i] = yin_buff[i] * i / sum;
	}
	yin_buff[0] = 1.0;
}


void Yin::evaluate_pitch()
{
	// Use the cmndf values in yin_buff to find the pitch/harmonic rate. result stored in class members pitch/harmonic_rates 
	int tau = tau_min;
	while (tau < tau_max)
	{
		if (yin_buff[tau] < harmonic_thresh)
		{
			while ((tau + 1 < tau_max) and (yin_buff[tau + 1] < yin_buff[tau]))
			{
				tau++;
			}
			pitch = (float)samplerate / tau;
			harmonic_rate = yin_buff[tau];
			return;
		}
		tau++;
	}
	// if no pitch is detected do this...
	pitch = 0.0;
	harmonic_rate = 1.0;
	for (int i = 0; i < tau_max; i++) // min(cmndf)
	{
		harmonic_rate = (yin_buff[i] < harmonic_rate) ? yin_buff[i] : harmonic_rate;
	}

	return;
}


void Yin::compute_yin(void* audioBuffer)
{
	load_buffer(audioBuffer);
	difference_function();
	cumulative_mean_normalized_difference_function();
	evaluate_pitch();
}


float Yin::get_pitch()
{
	return pitch;
}


float Yin::get_harmonic_rate()
{
	return harmonic_rate;
}
