//
// MIT License
//
// Copyright (c) 2017 Jetty
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// mcdRaw.c:
//		Decodes mcd raw files and converts to .wav
//

#include <stdio.h>
#include <strings.h>
#include <sys/syslimits.h>
#include "config.h"
#include "mcdRaw.h"



#define HEADER_ID			"MC_DataTool binary conversion"
#define SAMPLE_RATE_VAR			"Sample rate = "
#define HEADER_END			"EOH"
#define LINE_SIZE			1000
#define NUM_HEADER_LINES		8
#define STREAMS_VAR			"Streams = "
#define MAX_CS_STR			10
#define ELECTRODE_STEM			"El_"
#define SAMPLE_BUFFER_NUM_SAMPLES	50000

typedef short AdcDataType;

static int datafileChannelsCount = 0;
static int datafileChannels[MAX_CHANNELS];
static AdcDataType sampleBuffer[MAX_CHANNELS][SAMPLE_BUFFER_NUM_SAMPLES];
static int channelIndexes[MAX_CHANNELS];
static int channelIndexesCount = 0;
static int datafileSampleRate = 1;
static int outputSampleRate = 1;

struct WaveHeader
{
	char	riff[4];		// RIFF
	int	fileSize;		// 4 bytes - Fill in after creation (total filesize - 4)
	char	fileTypeHeader[4];	// WAVE
	char	format[4];		// fmt\0
	int	formatDataLen;		// always 16
	short	audioFormat;		// 1 = PCM
	short	numChannels;		// 1 = mono
	int	sampleRate;		// 44100
	int	byteRate;		// sampleRate * sampleBits * numChannels) / 8
	short	blockAlign;		// (sampleBits * numChannels) / 8
	short	sampleBits;		// 16 bits (4 bytes)
	char	dataHeader[4];		// data
	int	dataSize;		// Size of the audio data (bytes) (total filesize - 44)
};



// Removes carriage return / linefeeds from the line

static void removeCRLF(char *buffer)
{
	for ( int i = strlen(buffer) - 1; i >= 0; i -- )
	{
		if ( buffer[i] == '\r' || buffer[i] == '\n' )	buffer[i] = '\0';
		else						break;
	}
}



// Decodes the channel list from the header file
// Returns non-zero in case of error

static int decode_channels(char *channelsStr)
{
	int csIndex = 0;
	char *p = channelsStr;
	char cs[MAX_CS_STR + 1], *numberStart;

	do
	{
		// Next channel
		if ( ! *p || *p == ';' )
		{
			cs[csIndex] = '\0';
			csIndex = 0;
			if ( strncmp(ELECTRODE_STEM, cs, strlen(ELECTRODE_STEM)) == 0 )	numberStart = cs + strlen(ELECTRODE_STEM);

			if ( sscanf(numberStart, "%d", &datafileChannels[datafileChannelsCount ++]) != 1 )
			{
				fprintf(stderr, "Error: decode_channel, invalid channel number: %s\n", numberStart);
				return 1;
			}
		}
		else
		{
			cs[csIndex++] = *p;
			if ( csIndex >= MAX_CS_STR )
			{
				cs[csIndex] = '\0';
				fprintf(stderr, "Error: decode_channel, channel number too large: %s\n", cs);
				return 1;
			}
		}
	}
	while ( *(p++) );
	
	return 0;
}



// Read the header, returns 1 in case of error, otherwise 0

int mcdRaw_read_header(FILE *fp)
{
	char line[LINE_SIZE + 1];
	
	// Print out the header lines
	for ( int i = 0; i < NUM_HEADER_LINES; i ++ )
	{
		if ( ! fgets(line, LINE_SIZE, fp) )	{fprintf(stderr, "Error: Corrupt header: %d\n", i); return 1;}
		removeCRLF(line);

		if	( i == 0 )
		{
			if ( strcmp(line, HEADER_ID) != 0 )	{fprintf(stderr, "Error: First header line != %s\n", HEADER_ID); return 1;}
		}
		else if ( i == 3 )
		{
			if ( strncmp(SAMPLE_RATE_VAR, line, strlen(SAMPLE_RATE_VAR)) != 0 )
				{fprintf(stderr, "Error: Sample rate does not begin with: %s\n", SAMPLE_RATE_VAR); return 1;}
			if ( sscanf(line + strlen(SAMPLE_RATE_VAR), "%d", &datafileSampleRate) != 1 )
				{fprintf(stderr, "Error: Sample rate is not a number: %s\n", line); return 1;}
		}
		else if ( i == 6 )
		{
			if ( strncmp(STREAMS_VAR, line, strlen(STREAMS_VAR)) != 0 )
				{fprintf(stderr, "Error: List of channels does not begin with: %s\n", STREAMS_VAR); return 1;}
			if ( decode_channels(line + strlen(STREAMS_VAR) ) )	return 1;
			
			printf("# Channels in datafile: %d\n", datafileChannelsCount);
			//for ( int i = 0; i < datafileChannelsCount; i ++ )
			//	printf("Channel: <%d>\n", datafileChannels[i]);
		}
		else if ( i == 7 )
		{
			if ( strcmp(line, HEADER_END) != 0 )	{fprintf(stderr, "Error: Last header line != %s\n", HEADER_END); return 1;}
		}
		else	printf("%s\n", line);
	}

	return 0;
}



// Returns true if the datafile contains channel otherwise 0

int mcdRaw_contains_channel(int channel)
{
	for ( int i = 0; i < datafileChannelsCount; i ++ )
	{
		if ( channel == datafileChannels[i] )	return 1;
	}
	
	return 0;
}



// Prints the list of channels in the datafile to stderr

void mcdRaw_list_all_channels_on_stderr(void)
{
	fprintf(stderr, "Channel List: ");
	for ( int i = 0; i < datafileChannelsCount; i ++ )
		fprintf(stderr, "%d,", datafileChannels[i] );
	fprintf(stderr, "\n");
}



// Returns the incremental index of the requested channel in the datafile
// Returns -1 if not found

static int indexForChannel(int channel)
{
	for ( int i = 0; i < datafileChannelsCount; i ++ )
	{
		if ( channel == datafileChannels[i] )	return i;
	}

	return -1;
}



// Registers a channel for extraction

void mcdRaw_register_extract_channel(int channel)
{
	int ind = indexForChannel(channel);
	
	if ( ind >= 0 )	channelIndexes[channelIndexesCount++] = ind;
}



// Registers all channels for extraction

void mcdRaw_register_extract_all_channels(void)
{
	for ( int i = 0; i < datafileChannelsCount; i ++ )
	{
		mcdRaw_register_extract_channel(datafileChannels[i]);
	}
}



// Write the samples to the sample buffer

static void writeSampleBuffer(AdcDataType *samples, long sampleCount, float ampFactor)
{
	for ( int i = 0; i < channelIndexesCount; i ++ )
	{
		if ( ampFactor == 1.0 )	sampleBuffer[i][sampleCount] = samples[channelIndexes[i]];
		else			sampleBuffer[i][sampleCount] = (short)((float)samples[channelIndexes[i]] * ampFactor);
	}
}


#ifdef SWAP_ENDIAN

// Swap short from big endian to little endian

short swap_short( short val ) 
{
    return (val << 8) | ((val >> 8) & 0xFF);
}



// Swap int from big endian to little endian

int swap_int( int val )
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF ); 
    return (val << 16) | ((val >> 16) & 0xFFFF);
}

#else

short swap_short(short val)	{ return val; }
int swap_int(int val)		{ return val; }

#endif



// Write the .wav file header to disk
// Returns non-zero in case of error, otherwise 0

static int writeWaveHeader(FILE *fp, int sampleRate)
{
	long fileSize;
	struct WaveHeader hdr;

     	if ( fseek(fp, 0L, SEEK_END) )
	{
		perror("writeWaveHeader: Unable to write header");
		return 1;
	}

	fileSize = ftell(fp);

#ifdef DEBUG
	printf("Filesize: %ld\n", fileSize);
#endif

	strncpy(hdr.riff, 		  "RIFF", (size_t)4);
	hdr.fileSize			= swap_int((int)fileSize - 8);
	strncpy(hdr.fileTypeHeader, 	  "WAVE", (size_t)4);
	strncpy(hdr.format, 		  "fmt ", (size_t)4);
	hdr.formatDataLen		= swap_int(16);			// Always 16
	hdr.audioFormat 		= swap_short(1);		// PCM
	hdr.numChannels 		= swap_short(1);		// Mono
	hdr.sampleRate			= swap_int(sampleRate);	
	hdr.sampleBits			= swap_int(16);			// Bits per sample
	hdr.byteRate			= swap_int((hdr.sampleRate * (int)hdr.sampleBits * (int)hdr.numChannels) / 8);
	hdr.blockAlign			= swap_short((hdr.sampleBits * hdr.numChannels) / 8);
	strncpy(hdr.dataHeader, 	  "data", (size_t)4);
	hdr.dataSize			= swap_int((int)fileSize - 44);

	rewind(fp);
	if ( fwrite((void *)&hdr, sizeof(struct WaveHeader), 1, fp) != 1 )
	{
		perror("writeWaveHeader: Unable to write header");
		return 1;
	}

	return 0;
}



// Flushes the sample buffers to disk
// Returns non-zero in case of error, otherwise 0
// If finalizeWaveHeader is true, the wave header is written

static int flushSampleBuffers(size_t count, int finalizeWaveHeader, char *filenameStem, float speedFactor)
{
	char empty;
	FILE *fp, *fopen();
	char filename[PATH_MAX + 1];
	static int first = 1;

	for (int i = 0; i < channelIndexesCount; i ++ )
	{
		sprintf(filename, "%s_%d_S%0.0f.wav", filenameStem, datafileChannels[channelIndexes[i]], speedFactor);
		if ( (fp  = fopen(filename, ( first ) ? "w" : (finalizeWaveHeader) ? "r+" : "a")) == NULL)
		{
			perror(filename);
			return 1;
		}

		// If we've just opened the file, write a blank space for the wave file header
		if ( first )
		{
     			if ( fwrite((void *)&empty, (size_t)1, sizeof(struct WaveHeader), fp) != sizeof(struct WaveHeader) )
			{
				fprintf(stderr, "Error: short write: %s\n", filename);
				fclose(fp);
				return 1;
			}
		}

		if	( finalizeWaveHeader )
		{
			if ( writeWaveHeader(fp, outputSampleRate) )
			{
				fclose(fp);
				return 1;
			}

		}
     		else if ( fwrite((void *)sampleBuffer[i], sizeof(AdcDataType), count, fp) != count )
		{
			fprintf(stderr, "Error: short write: %s\n", filename);
			fclose(fp);
			return 1;
		}

		fclose(fp);
	}

	first = 0;

	return 0;
}



// Returns true in case of error, otherwise false

int mcdRaw_extract(FILE *fp, float ampFactor, float speedFactor, char *filename)
{
	size_t n;
	AdcDataType samples[MAX_CHANNELS];
	long totalSampleCount = 0L, sampleCount = 0L;

	if ( sizeof(AdcDataType) != 2 )
	{
		fprintf(stderr, "Error: short != 16 bits on this architecture, find a 16 bit signed type\n");
		return 1;
	}

#ifdef DEBUG
	for ( int i = 0; i < channelIndexesCount; i ++ )
		printf("Channel Index: %d\n", channelIndexes[i]);
#endif

	if ( speedFactor == 1.0 )	outputSampleRate = datafileSampleRate;
	else				outputSampleRate = (int)((float)datafileSampleRate * speedFactor);
	
	int failedRead = 0;
	do
	{
		n = fread((void *)samples, sizeof(AdcDataType), (size_t)datafileChannelsCount, fp);

		if ( n != (size_t)datafileChannelsCount ) failedRead = 1;

		if ( ! failedRead )
		{
			//Write samples to the buffers
			writeSampleBuffer(samples, sampleCount, ampFactor);
			sampleCount ++;
			totalSampleCount ++;
		}

		if ( failedRead || sampleCount >= SAMPLE_BUFFER_NUM_SAMPLES )
		{
			// Write buffers to the files
			if ( flushSampleBuffers((size_t)sampleCount, 0, filename, speedFactor) ) return 1;
			sampleCount = 0;
		}
	}
	while ( ! failedRead );

	if ( n != 0 )
	{
		fprintf(stderr, "Error: End of file reached prematurely, file is likely corrupt: %ld samples != %d\n", n, datafileChannelsCount);
		//return 1;
	}

	if ( ampFactor != 1.0 )	printf("Signal amplified: %0.2fX\n", ampFactor);
	printf("Total Sample Count: %ld\n", totalSampleCount);
	printf("Input Sample Rate: %d\n", datafileSampleRate);
	printf("Output Sample Rate: %d\n", outputSampleRate);
	printf("Input Duration: %0.2f seconds\n", (double)totalSampleCount / (double) datafileSampleRate);
	printf("Output Duration: %0.2f seconds\n", (double)totalSampleCount / (double) outputSampleRate);

	if ( flushSampleBuffers(0, 1, filename, speedFactor) ) return 1;

	return 0;
}
