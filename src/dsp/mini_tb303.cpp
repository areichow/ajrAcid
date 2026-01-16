#include "mini_tb303.h"

#include <math.h>
#include <stdlib.h>

namespace {
const char* const kOscillatorOptions[] = {"saw", "sqr", "super", "juno", "analog", "pwm"};
}

ChamberlinFilter::ChamberlinFilter(float sampleRate) : _lp(0.0f), _bp(0.0f), _sampleRate(sampleRate) {
  if (_sampleRate <= 0.0f) _sampleRate = 44100.0f;
}

void ChamberlinFilter::reset() {
  _lp = 0.0f;
  _bp = 0.0f;
}

void ChamberlinFilter::setSampleRate(float sr) {
  if (sr <= 0.0f) sr = 44100.0f;
  _sampleRate = sr;
}

float ChamberlinFilter::process(float input, float cutoffHz, float resonance) {
  // tuning coefficient (chamberlin filter): f = 2*sin(pi*fc/fs); clamp to safe range
  float f = 2.0f * sinf(3.14159265f * cutoffHz / _sampleRate);
  if (!isfinite(f))
  	f = 0.0f;
  if (f > 1.9f) f = 1.9f; // avoid extreme integrator gain
  if (f < 0.0f) f = 0.0f;

  // new resonance mapping: 0 -> essentially no resonance; 1 -> very resonant but bounded.
  // we map resonance onto a damping factor q in (0..1]. Larger q = more damping (less resonance)
  // using a curved response so the lower half of the knob covers gentle musical range
  float r = resonance;
  if (r < 0.0f) r = 0.0f;
  if (r > 0.995f) r = 0.995f; // keep some margin from self-oscillation
  float q = 1.0f - (r * 0.96f); // r=0 -> q≈1.0 (flat), r≈1 -> q≈0.04 (high resonance)
  if (q < 0.01f) q = 0.01f;

  float hp = input - _lp - q * _bp;
  _bp += f * hp;
  _lp += f * _bp;

  // more gentle saturation for less squelch - orig was 1.3; ajr v1 0.9
  _bp = tanhf(_bp * 1.1f);
  const float kStateLimit = 20.0f;
  if (_lp > kStateLimit) _lp = kStateLimit;
  if (_lp < -kStateLimit) _lp = -kStateLimit;
  if (_bp > kStateLimit) _bp = kStateLimit;
  if (_bp < -kStateLimit) _bp = -kStateLimit;

  // Output gain compensation: reduce level as resonance increases to avoid harshness
  // float gainComp = 1.0f - (resonance * 0.45f);
  // if (gainComp < 0.2f) gainComp = 0.2f;
  // return _lp * gainComp;
  return _lp;
}

TB303Voice::TB303Voice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f),
    nyquist(0.0f),
    filter(sampleRate) {
  setSampleRate(sampleRate);
  reset();
}

void TB303Voice::reset() {
  initParameters();
  phase = 0.0f;
  for (int i = 0; i < kSuperSawOscCount; ++i) {
    float seed = (static_cast<float>(i) + 1.0f) * 0.137f;
    superPhases[i] = seed - floorf(seed);
  }
  freq = 110.0f;
  targetFreq = 110.0f;
  slideSpeed = 0.001f;
  env = 0.0f;
  gate = false;
  slide = false;
  amp = 0.3f;
  
  junoPhaseA = 0.0f; // initialize Juno phases and slow modulation phase
  junoPhaseB = 0.5f; // offset for stereo-like feel
  junoModPhase = 0.0f;
  filter.reset();
}

void TB303Voice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  nyquist = sampleRate * 0.5f;
  filter.setSampleRate(sampleRate);
}

void TB303Voice::startNote(float freqHz, bool accent, bool slideFlag) {
  slide = slideFlag;

  if (!slide) {
    freq = freqHz;
  }
  targetFreq = freqHz;

  gate = true;
  // smaller boost than before (was 2.0f vs 1.0f) to decrease squelch
  // orig:
  // env = accent ? 2.0f : 1.0f;
  // ajr orig - 
  // env = accent ? 1.45f : 1.0f;
  // let's try a little more
  env = accent ? 1.7f : 1.0f;
}

void TB303Voice::release() { gate = false; }


// polyblep helper: band-limit discontinuities for saw edges
static inline float poly_blep(float t, float dt) {
	if (dt <= 0.0f) return 0.0f;
	if (t < dt) {
		float x = t / dt;
		return x + x - x * x - 1.0f;
	}
	if (t > 1.0f - dt) {
		float x = (t - 1.0f) / dt;
		return x * x + x + x + 1.0f;
	}
	return 0.0f;
}
float TB303Voice::oscSaw() {
  phase += freq * invSampleRate;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return 2.0f * phase - 1.0f;
}

float TB303Voice::oscSquare(float saw) {
  return saw >= 0.0f ? 1.0f : -1.0f;
}

float TB303Voice::oscSuperSaw() {
  static const float kSuperSawDetune[kSuperSawOscCount] = {
    -0.019f, 0.019f, -0.012f, 0.012f, -0.0065f, 0.0065f
  };

  float basePhaseInc = freq * invSampleRate;
  phase += basePhaseInc;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }

  float sum = 2.0f * phase - 1.0f;

  for (int i = 0; i < kSuperSawOscCount; ++i) {
    float detunedFreq = freq * (1.0f + kSuperSawDetune[i]);
    float inc = detunedFreq * invSampleRate;
    superPhases[i] += inc;
    if (superPhases[i] >= 1.0f) {
      superPhases[i] -= floorf(superPhases[i]);
    } else if (superPhases[i] < 0.0f) {
      superPhases[i] += 1.0f;
    }
    sum += 2.0f * superPhases[i] - 1.0f;
  }

  // constexpr float kGain = 1.0f / (1.0f + TB303Voice::kSuperSawOscCount);
  constexpr float kGain = 1.0f / (TB303Voice::kSuperSawOscCount - 5);
  return sum * kGain;
}

// juno-style saw (two detuned saws with subtle low-rate modulation)
float TB303Voice::oscJunoSaw() {
  float lfoHz = 0.6f;
  junoModPhase += lfoHz * invSampleRate;
  if (junoModPhase >= 1.0f) junoModPhase -= 1.0f;
  float lfo = sinf(2.0f * 3.14159265f * junoModPhase);

  float baseInc = freq * invSampleRate;
  phase += baseInc;
  if (phase >= 1.0f) phase -= 1.0f;
  float baseSaw = 2.0f * phase - 1.0f;

  // Subtle chorus-like detune around +/- ~0.4% with a tiny LFO sway
  float detA = 0.004f + 0.003f * lfo;     // +0.4% +/- 0.3%
  float detB = -0.004f + 0.003f * (-lfo); // -0.4% -/+ 0.3%
  float incA = freq * (1.0f + detA) * invSampleRate;
  float incB = freq * (1.0f + detB) * invSampleRate;
  junoPhaseA += incA;
  junoPhaseB += incB;
  if (junoPhaseA >= 1.0f) junoPhaseA -= floorf(junoPhaseA);
  if (junoPhaseB >= 1.0f) junoPhaseB -= floorf(junoPhaseB);
  float sawA = 2.0f * junoPhaseA - 1.0f;
  float sawB = 2.0f * junoPhaseB - 1.0f;
  float dtA = incA; if (dtA > 1.0f) dtA = 1.0f;
  float dtB = incB; if (dtB > 1.0f) dtB = 1.0f;
  sawA -= poly_blep(junoPhaseA, dtA);
  sawB -= poly_blep(junoPhaseB, dtB);

  float sum = baseSaw + sawA + sawB;
  return sum * (1.0f / 3.0f) * 1.3f;
}


float TB303Voice::oscAnalogSaw() {
    // very light drift & tilt for "analog" character (mono-friendly)
    // reuse junoModPhase as slow LFO; keep the engine diff minimal
    float lfoHz = 0.45f;
    junoModPhase += lfoHz * invSampleRate; if (junoModPhase >= 1.0f) junoModPhase -= 1.0f;
    float lfo = sinf(2.0f * 3.14159265f * junoModPhase);

    // small per-sample frequency flutter (subtle)
    float flutter = 1.0f + 0.00025f * lfo;
    float inc = freq * invSampleRate * flutter;
    phase += inc; if (phase >= 1.0f) phase -= 1.0f;

    // naive saw
    float x = 2.0f * phase - 1.0f;
    // gentle "tilt" for analog core curvature
    x += 0.10f * x * x;

    // polyblep to band-limit the edge at phase wrap
    float dt = inc; if (dt > 1.0f) dt = 1.0f;
    x -= poly_blep(phase, dt);

    // tiny extra warmth; keep conservative
    return x * 1.05f;
}

float TB303Voice::oscPWM() {
    // juno-like PWM: slow LFO modulates duty; band-limit both edges
    float lfoHz = 0.6f;
    junoModPhase += lfoHz * invSampleRate; if (junoModPhase >= 1.0f) junoModPhase -= 1.0f;
    float lfo = sinf(2.0f * 3.14159265f * junoModPhase);

    // 40–60% duty, softly animated
    float pwm = 0.5f + 0.10f * lfo;

    // advance base phase
    float inc = freq * invSampleRate;
    phase += inc; if (phase >= 1.0f) phase -= 1.0f;
    float dt = inc; if (dt > 1.0f) dt = 1.0f;

    // naive square: -1 .. +1
    float y = (phase < pwm ? 1.0f : -1.0f);

    // polyBLEP corrections: one at 0 (rising), one at pwm (falling)
    y += poly_blep(phase, dt);
    float t2 = phase - pwm; if (t2 < 0.0f) t2 += 1.0f;
    y -= poly_blep(t2, dt);

    // slightly fuller level to feel "Juno square"-ish; keep headroom
    return y * 0.95f;
}

float TB303Voice::oscillatorSample() {
  int oscIdx = oscillatorIndex();
  if (oscIdx == 1) {
    float saw = oscSaw();
    return oscSquare(saw);
  }
  if (oscIdx == 2) {
    return oscSuperSaw();
  }
  if (oscIdx == 3) {
  	return oscJunoSaw();
  }
  if (oscIdx == 4) {
      return oscAnalogSaw();
  }
  if (oscIdx == 5) {
      return oscPWM();
  }
  return oscSaw();
}

float TB303Voice::svfProcess(float input) {
  // Slide toward target frequency
  freq += (targetFreq - freq) * slideSpeed;
  if (!isfinite(freq))
    freq = targetFreq;

  // Envelope decay
  if (gate || env > 0.0001f) {
    float decayMs = parameterValue(TB303ParamId::EnvDecay);
    float decaySamples = decayMs * sampleRate * 0.001f;
    if (decaySamples < 1.0f)
      decaySamples = 1.0f;
    // 0.01 represents roughly -40 dB, a practical "off" point for the envelope.
    constexpr float kDecayTargetLog = -4.60517019f; // ln(0.01f)
    float decayCoeff = expf(kDecayTargetLog / decaySamples);
    env *= decayCoeff;
  }

  // Accent-derived gentle drive: slightly warm the input when envelope is higher
  float drive = 1.0f + 0.08f * (env - 1.0f);
  if (drive < 1.0f) drive = 1.0f;
  float driven = tanhf(input * drive);

  float cutoffHz = parameterValue(TB303ParamId::Cutoff) + parameterValue(TB303ParamId::EnvAmount) * env;
  if (cutoffHz < 50.0f)
    cutoffHz = 50.0f;
  float maxCutoff = nyquist * 0.9f;
  if (cutoffHz > maxCutoff)
    cutoffHz = maxCutoff;


  return filter.process(input, cutoffHz, parameterValue(TB303ParamId::Resonance));
}

float TB303Voice::process() {
  if (!gate && env < 0.0001f) {
    return 0.0f;
  }

  float osc = oscillatorSample();
  float out = svfProcess(osc);

  return out * amp;
}

const Parameter& TB303Voice::parameter(TB303ParamId id) const {
  return params[static_cast<int>(id)];
}

void TB303Voice::setParameter(TB303ParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

void TB303Voice::adjustParameter(TB303ParamId id, int steps) {
  params[static_cast<int>(id)].addSteps(steps);
}

float TB303Voice::parameterValue(TB303ParamId id) const {
  return params[static_cast<int>(id)].value();
}

int TB303Voice::oscillatorIndex() const {
  return params[static_cast<int>(TB303ParamId::Oscillator)].optionIndex();
}

void TB303Voice::initParameters() {
  params[static_cast<int>(TB303ParamId::Cutoff)] = Parameter("cut", "Hz", 40.0f, 3000.0f, 700.0f, (3000.f - 40.0f) / 128);
  params[static_cast<int>(TB303ParamId::Resonance)] = Parameter("res", "", 0.0f, 0.95f, 0.25f, (0.95f - 0.0f) / 128);
  params[static_cast<int>(TB303ParamId::EnvAmount)] = Parameter("env", "Hz", 0.0f, 1800.0f, 250.0f, (1800.0f - 0.0f) / 128);
  params[static_cast<int>(TB303ParamId::EnvDecay)] = Parameter("dec", "ms", 20.0f, 2200.0f, 420.0f, (2200.0f - 20.0f) / 128);
  params[static_cast<int>(TB303ParamId::Oscillator)] = Parameter("osc", "", kOscillatorOptions, 6, 0);
  params[static_cast<int>(TB303ParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);
}