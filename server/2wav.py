import sys
import wave

channels = 2 if len(sys.argv) < 2 else int(sys.argv[1])
rate = 16000 if len(sys.argv) < 5 else int(sys.argv[4])
    
with open('i2s.raw', 'rb') as raw:
    wav = wave.open('recording.wav', 'wb')
    wav.setframerate(rate)
    wav.setsampwidth(2)
    wav.setnchannels(1)
    wav.writeframes(raw.read())
    wav.close()