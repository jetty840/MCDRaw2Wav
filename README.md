# MCDRaw2Wav

Converts a raw file output by Multichannel Systems MC_Rack / MC_DataTool software into seperate .wav audio files with optional amplification, speed change and channel extraction.

This enables the leveraging of the multitude of audio software available for audio playback, filtering and spectrum analysis etc.

## Getting Started

The software is written for Unix and has been compiled / tested under macOS.  Will work on other unix systems with minimal or no porting.

### Prerequisites

* MC_DataTool to export Raw data in binary format
* gcc
* make

### Installing

Compile the software:

```
make
```

Export raw file:

```
* Start MC_DataTool

* Load .mcd file

* Export BIN file with "All Channels" and options "Header" and "Signed"
```

Use MCDRaw2Wav to generate .wav files:

```
Extract channels 43 28 55 57 and output as .wav files with amplification of 40X and a speed up of 4X:

./MCDRaw2Wav -a 40 -s 4 filename.raw 43 28 55 57


Extract all channels and output as .wav files with no amplification and no speed up:

./MCDRaw2Wav filename.raw


Output .wav files are saved as filename.raw_Channel#_SspeedFactor.wav. If you specified a filename in a different folder, the output files will be saved in this folder.
```

Example output log:

```
$ ./MCDRaw2Wav -a 40 -s 4 example.raw 43 28 55 57
Version 2.6.15
MC_REC file = "C:\Users\Jetty\Desktop\example.mcd"
ADC zero = 32768
El = 0.11528?V/AD
# Channels in datafile: 60
Signal amplified: 40.00X
Total Sample Count: 17885000
Input Sample Rate: 50000
Output Sample Rate: 200000
Input Duration: 357.70 seconds
Output Duration: 89.42 seconds
```

## Authors

* **Jetty** - *Initial work*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Acknowledgments

* Innovation4Health

