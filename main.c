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
// main.c:
//		Decodes a Raw File(with header and Signed 16 bit values) from MC_DataTool into individual channels,
//		output as .wav audio files
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syslimits.h>
#include <ctype.h>
#include <strings.h>
#include "config.h"
#include "mcdRaw.h"



void usage(char *command)
{
	fprintf(stderr, "Usage:   %s [-a AmpFactor] [-s SpeedFactor] rawFilename [Channel# A] [Channel# B] .. [Channel# N]\n", command);
	fprintf(stderr, "Example: %s -a 1.5 -s 5 filename.raw 14 35 28\n", command);
	fprintf(stderr, "         Extracts channels 14, 35 and 28, amplifies by 1.5X and speeds up by 5X outputting as a .wav file\n");
}



int main(int argc, char **argv)
{
	int c;
	FILE *fp, *fopen();
	int channelCount = 0;
	char filename[PATH_MAX+1];
	int channels[MAX_CHANNELS];
	float ampFactor = 1.0, speedFactor = 1.0;

	while ( (c = getopt(argc, argv, "a:s:")) != -1 )
	{
		switch (c)
		{
			case 'a':
					ampFactor = atof(optarg);
					break;

			case 's':
					speedFactor = atof(optarg);
					break;

			case '?':
					if 	( optopt == 'a' || optopt == 's' )	fprintf(stderr, "Option -%c requires an argument.\n", optopt);
					exit(-1);

			default:
					abort();	
		}
	}

#ifdef DEBUG
	printf("AmpFactor: %f\n", ampFactor);
	printf("SpeedFactor: %f\n", speedFactor);
#endif

	if ( argc == 1 )	{usage(argv[0]); exit(-1);}

	if ( ampFactor <= 0.0 || speedFactor <= 0.0 )	{usage(argv[0]); exit(-1);}

	if ( optind < argc )	strcpy(filename, argv[optind++]);
	else			{usage(argv[0]);exit(-1);}

	if (( fp = fopen(filename, "r" )) == NULL)	{perror(filename); exit(-2);}

	// Parse channels numbers
	for ( int i = optind; i < argc; i ++ )
	{
		if ( channelCount >= MAX_CHANNELS )	{usage(argv[0]); exit(-1);}

		if ( ( sscanf(argv[i], "%d", &channels[channelCount]) != 1 ) || 
		     channels[channelCount] < 0 || 
		     channels[channelCount] >= MAX_CHANNELS )
		{
			fprintf(stderr, "Error: Channel must be a positive integer within the range of available channels, e.g. 34\n");
			fclose(fp);
			usage(argv[0]);
			exit(-3);
		}
	
		channelCount ++;
	}

	if ( mcdRaw_read_header(fp) )	{fclose(fp); exit(-3);}

	// Validate all channel numbers requested exist in the datafile and register them for extaction
	for ( int i = 0; i < channelCount; i ++ )
	{
		if ( ! mcdRaw_contains_channel( channels[i] ) )
		{
			fprintf(stderr, "Error: Datafile %s does not contain channel: %d\n", argv[1], channels[i] );
			fclose(fp);
			mcdRaw_list_all_channels_on_stderr();
			exit(-4);
		}

		mcdRaw_register_extract_channel(channels[i]);
	}

	// If we haven't specified any channels, we extract everything
	if ( channelCount == 0 )	mcdRaw_register_extract_all_channels();

	mcdRaw_extract(fp, ampFactor, speedFactor, filename);

	fclose(fp);

	exit(0);
}
