"""The speeder's engine sounds: three loops the plugin crossfades, three
one-shots it plays on changes of speed, and the start-up and shutdown it
plays as Kliff gets on and off.

    py -3 speeder/make_engine.py ["Speeder Bike.wav"]

Given the Star Wars SFX Archive's "Speeder Bike.wav" (48 kHz float, laid
out as its video's chapters, each take set apart by silence):

    engine_idle    one period of the idle bed from 1:44, which repeats
                   sample for sample every 478,879 samples
    engine_close   12 s of the close bed from 3:22
    engine_boost   12 s of the boost bed from 0:32
    accelerate     the acceleration take, 0:58.5 to 1:03.5
    boost_start    the boost's start, 0:23.1 to 0:28.7
    boost_stop     the boost's end, 0:54.3 to 0:57.3
    startup        the start-up, 0:02.35 to 0:05.05, its pitch climbing
                   from about 360 Hz to 1,100 Hz (the take at 0:01 is a
                   short burst and is left out)
    shutdown       the shutdown, 0:07.15 to 0:08.5, its pitch falling

The close and boost beds do not repeat exactly, so each loop's last second
is crossfaded into its start. Without the WAV the three loops are a
synthesised hum and the one-shots are silent.

Writes mod/assets/sound/<name>.raw, mono 16-bit PCM at 24 kHz, which
src/speeder.rc carries as resources 301 to 308 in the order above, and WAV
copies in speeder/out for listening. Every sound shares one scale, so the
takes keep their loudness relative to each other.
"""

import os
import struct
import sys
import wave

import numpy as np

RATE = 24000
HERE = os.path.dirname(os.path.abspath(__file__))
SOUND = os.path.join(HERE, "..", "mod", "assets", "sound")
OUT = os.path.join(HERE, "out")

IDLE_START, IDLE_PERIOD = 104.0, 478879     # seconds, samples at 48 kHz
LOOPS = {"engine_close": 202.0, "engine_boost": 32.0}   # start, seconds
LOOP_SECONDS, FADE = 12.0, 1.0
SHOTS = {"accelerate": (58.5, 63.5), "boost_start": (23.1, 28.7), "boost_stop": (54.3, 57.3),
         "startup": (2.35, 5.05), "shutdown": (7.15, 8.5)}
ORDER = ("engine_idle", "engine_close", "engine_boost", "accelerate", "boost_start", "boost_stop", "startup",
         "shutdown")


def read_wav(path):
    """(mono float samples, rate) of a PCM or float WAV."""
    d = open(path, "rb").read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a WAV file")
    o, fmt, data = 12, None, None
    while o + 8 <= len(d):
        cid, n = d[o:o + 4], struct.unpack_from("<I", d, o + 4)[0]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", d, o + 8)
        elif cid == b"data":
            data = d[o + 8:o + 8 + n]
        o += 8 + n + (n & 1)
    if fmt is None or data is None:
        raise ValueError(f"{path} has no fmt or data chunk")
    tag, channels, rate, _rate, _align, bits = fmt
    if (tag, bits) == (3, 32):
        a = np.frombuffer(data, "<f4").astype(np.float64)
    elif (tag, bits) == (1, 16):
        a = np.frombuffer(data, "<i2") / 32768.0
    else:
        raise ValueError(f"WAV format {tag} at {bits} bits is not read here")
    return a.reshape(-1, channels).mean(axis=1), rate


def resample(x, rate):
    """`x` at RATE, resampled in the frequency domain. That treats it as
    periodic, which keeps a loop's seam seamless; a one-shot is faded to
    silence at both ends first, so its ends do not bleed into each other."""
    n = round(len(x) * RATE / rate)
    spec = np.fft.rfft(x)[:n // 2 + 1]
    return np.fft.irfft(spec, n) * n / len(x)


def loop(x, rate, start):
    s, n, f = int(start * rate), int(LOOP_SECONDS * rate), int(FADE * rate)
    out = x[s:s + n + f].copy()
    t = np.linspace(0.0, np.pi / 2, f)
    out[:f] = out[:f] * np.sin(t) + out[n:n + f] * np.cos(t)   # equal power
    return resample(out[:n], rate)


def shot(x, rate, start, end):
    out = x[int(start * rate):int(end * rate)].copy()
    a, b = int(0.01 * rate), int(0.08 * rate)
    out[:a] *= np.linspace(0.0, 1.0, a)
    out[-b:] *= np.linspace(1.0, 0.0, b)
    return resample(out, rate)


def from_movie(path):
    x, rate = read_wav(path)
    if rate != 48000:
        raise ValueError(f"expected the 48 kHz file, this one is {rate} Hz")
    s = int(IDLE_START * rate)
    sounds = {"engine_idle": resample(x[s:s + IDLE_PERIOD], rate)}
    sounds.update({name: loop(x, rate, start) for name, start in LOOPS.items()})
    sounds.update({name: shot(x, rate, a, b) for name, (a, b) in SHOTS.items()})
    return sounds


def synthesised():
    seconds = 2.0
    t = np.arange(int(RATE * seconds)) / RATE
    tone = lambda hz: max(1, round(hz * seconds)) / seconds   # whole cycles per loop
    base = tone(62)
    buzz = sum(np.sin(2 * np.pi * base * k * t) / k ** 1.1 for k in range(1, 13))
    buzz *= 0.75 + 0.25 * np.sin(2 * np.pi * tone(14) * t)
    whine = np.sin(2 * np.pi * tone(1180) * t) + 0.6 * np.sin(2 * np.pi * tone(1187) * t)
    spec = np.fft.rfft(np.random.default_rng(1983).standard_normal(len(t)))
    f = np.fft.rfftfreq(len(t), 1 / RATE)
    spec[(f < 300) | (f > 3500)] = 0
    air = np.fft.irfft(spec, len(t))
    hum = 0.55 * buzz / np.abs(buzz).max() + 0.06 * whine + 0.14 * air / np.abs(air).max()
    silence = np.zeros(RATE // 10)
    return {"engine_idle": hum, "engine_close": hum, "engine_boost": hum,
            "accelerate": silence, "boost_start": silence, "boost_stop": silence, "startup": silence,
            "shutdown": silence}


def write(name, x, peak):
    pcm = np.round(np.clip(x / peak * 0.9, -1, 1) * 32767).astype("<i2")
    with open(os.path.join(SOUND, name + ".raw"), "wb") as f:
        f.write(pcm.tobytes())
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"{name}: {len(pcm) / RATE:.2f} s, {len(pcm) * 2} bytes")


def main():
    os.makedirs(SOUND, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    sounds = from_movie(sys.argv[1]) if len(sys.argv) > 1 else synthesised()
    peak = max(np.abs(x).max() for x in sounds.values())
    for name in ORDER:
        write(name, sounds[name], peak)


main()
