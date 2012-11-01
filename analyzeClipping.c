#include <stdio.h>
#include <stdlib.h>
#include <sndfile.h>







// MERGE THIS WITH main_sre.c
// So that we have proper label output, single channel processing,
// frame-based and sequential sample


/********
// Change frame-based section so that it outputs labels and checks overlapping clipping between frames
*******/





// NOTE!!!
// Change the following line:
//		if ((pfBuffer[j] == fMax) || (iMaxFound && (pfBuffer[j] > .999))) {
// to replace .999 with 'Max - (Max * alpha)'
// where alpha is the percentage error we want to account for in the clipping detection

// Also combine the different approaches into one procedure
// Sequential Sample
// Frame based
// ICE/MCB

// Change code to only process a single channel, command-line input

// Add user-definable settings for definition of an MCB?

// Add options for things! with defaults!  IN the flag/parsing section

// Add/combine in CSR???? later??






/* Function to print usage information */
void usage(char *argv[])
{
	printf("Usage:\n %s [-f] [-A|B] <input filename> <filename for clip labels>\n", argv[0]);
	printf("\nOptions:\n");
	printf("\t-f\t:\tUse frame-based processing to detect clipping.  Omitting -f will use sequential-sample based peak clipping detection.  Optional.\n");
	printf("\t-[AB]\t:\tAnalyze the first channel of the audio file if A, second if B.  If omitted, program defaults to channel 0.  Optional.\n");
	printf("\n\tFinal two input arguments must be the path and name of the audio file to be analyzed, and the desired destination for the clipping labels, respectively.  Both are required.\n");
	printf("\n\nPossibly Obsolete Warning?\nDisclaimer: For frame-based processing, the number of clipped frames for each event type may not be accurate!\nThis" 
			" is because it currently counts each frame from each channel as a separate frame, making it possible for clipping events" 
			" that occur at the same time on different channels to be counted more than once.\n\n");
	exit (8);
}

/* Main */
int main(int argc, char *argv[] )
{
	int iDoFrameProcessing = 0;
	int iStartChannel = 0;
	
	// Print usage statement if incorrect # of arguments
	if ((argc != 3) && (argc != 4) && (argc != 5)) {
		usage(argv);
	}

	// Parse command line arguments
	while ((argc > 1) && (argv[1][0] == '-')) {
		switch (argv[1][1])
		{
			case 'f':
				printf("%s\n",&argv[1][2]);
				iDoFrameProcessing = 1;
				break;
				
			case 'A':
				iStartChannel = 0;
				break;
			
			case 'B': 
				iStartChannel = 1;
				break;

			default:
				printf("Invalid Argument: %s\n", argv[1]);
				usage(argv);
		}

		++argv;
		--argc;
	}

	
	// Variable Initialization
    SNDFILE *sf;   // Sndfile structure for audio file
    SF_INFO info;  // Holds header information
    int iReadCount, iNumItems;
    float *pfBuffer;
    int iFrames,iSampleRate,iChannels;
    int iClips = 0;
    int iTotalClips = 0;
    int i, j, k;
    int iMaxFound, iMinFound = 0;  // Flags set if the maximum or minimum has been reached
    int iFormat, iBitrate;
    int iPrint = 0;
	float fMax, fMin;
   
   	/** Frame-based variables  **/
	float fFrameLength = .030; // 30 ms  
	float fMCBThreshold = .35;  // 35% of a frame must be clipped to be considered an MCB 
								// Previously 50%
	int iNumFrames; // Number of frames to be processed 
	int iFrameLength; // = (int) (fFrameLength  * iSampleRate);
	int offset; // Variable to be used to make index calculations easier
	float fPercentClipped;
	int iTotalICE = 0;
	int iTotalMCB = 0;
	int iFrameClips = 0;
   	
    /** Sequential processing variables  **/
    // Major Clip Block Length
    // tentative length is 100ms = .100 s
    // This is only temporary and must be experimentally determined 
	//float fBlockLength = 0.100;
	float fBlockLength = 0.010; // Temporarily set very small for testing
	int iBlockSamples; // Number of samples necessary for a major clip block
						// Sample rate dependent
						
	FILE *fLog; 
	
	// FOR TEMP LABELS!!!
	// 
	//
	//
	float fStartTime = 0;
	float fEndTime = 0;

	// Clear some space in the Terminal for readability of output
	printf("\n\n\n\n\n\n\n\n");

    // Open the WAV file, read and parse header
    info.format = 0;
    sf = sf_open(argv[1] , SFM_READ, &info);
  	
  	// Print warning if something went wrong
    if (sf == NULL) {
        printf("Failed to open the file.\n");
        exit(-1);
    }
    
    // Print some of the header info, and figure out how much data to read. 
    iFrames = info.frames;
    iSampleRate = info.samplerate;
    iChannels = info.channels;
    iNumItems = iFrames * iChannels;
    printf("Frames = %d\n",iFrames);
    printf("SampleRate = %d\n",iSampleRate);
    printf("Channels = %d\n",iChannels);
    printf("NumberOfItemsToRead = %d\n",iNumItems);
    
    // If the channel of interest is B, and there's only one channel, error
    if (iChannels < iStartChannel + 1) {
    	printf("Channel %d is not valid! There are only %d channel(s). Error.\n", iStartChannel + 1, iChannels);
    	exit(1);
    }
    
    
    // Use the sample rate to calculate the number of clipped
    // samples in a row necessary for a major clip block
    iBlockSamples = iSampleRate * fBlockLength;
    // printf("fBlockLength = %f, iBlockSamples = %d\n", fBlockLength, iBlockSamples);

    
    // Allocate space for the data to be read, then read it 
    // Reads entire file at once, may not work for larger files
    pfBuffer = (float *) malloc(iNumItems * sizeof(float));
    iReadCount = sf_read_float(sf, pfBuffer, iNumItems);

    // Close file
    sf_close(sf);

	// Check the number of samples read, print warning if unexpected outcome
    if (iReadCount < iNumItems) {
        printf("End of file reached before expected.\nRead %d items, expected %d.\n", iReadCount, 
        		iNumItems);
    } else {
        //printf("Read %d items.\n", iReadCount);
    }

	// Determine the max and min sample values program
	// is searching for
	fMax = -1.0;
	fMin = 1.0;
	
	for (i = 0; i < iReadCount; i++) {
		if (pfBuffer[i] > fMax)
			fMax = pfBuffer[i];
		if (pfBuffer[i] < fMin)
			fMin = pfBuffer[i];
	} 
	
	/*  // Used for verification
	printf("MaxFound %f\nExpectedMax: 1.0\n", fMax);
	printf("MinFound %f\nExpectedMin: -1.0\n", fMin);
	if (fMax < 1) {
		printf("Using discovered maximum to detect clipping.\n");
	} 
	if (fMin > -1) {
		printf("Using discovered minimum to detect clipping.\n");
	}
	*/
	
	
	
	
	
	
	
	
	
	
	
	if (iDoFrameProcessing) {
		
		/********************   Frame Based Processing   ********************/	
		iFrameLength = (int) (fFrameLength  * iSampleRate);
		iNumFrames = iFrames / iFrameLength; // iFrames is actually the number of samples, despite the misleading name used by libsndfile
					// Do I need to round this??
					//printf("1\n");
		//printf("numFrames %d frames %d framelength %d\n",iNumFrames,  iFrames, iFrameLength);

		fLog = fopen(argv[2],"a+"); /* apend file (add text to a file or create a file if it does not exist.*/ 

		/* For each frame */
		for (i = 0; i < iNumFrames; ++i) {
			/* For each channel */
			//printf("2\n");
			
			
			
			
			// NOTE!!! 
			//    This file was modified to only do ONE channel from the input audio file
			//    This is because the NIST lists specify A or B as the channel of interest
			//    Instead of looping through each channel here, j is assigned to the desired
			//    channel, according to the input flags.  Defaults to channel 0.
			
			
			//for (j = 0; j < iChannels; ++j) {
			
			j = iStartChannel;
			
				/* For the samples of that channel for the given frame */
				//printf("3\n");
				for (k = j; k < iFrameLength * iChannels; k+=iChannels) {	
					//printf("4\n");
					/* Index = (i * iFrameLength * iChannels) + k 
							 = offset + k   */
					offset = i * iFrameLength * iChannels;
					
					/* If the maximum value is found -OR-
						   it has previously been found and current
						   sample is over threshold (currently .999),
					   Set flag that max has been found, or update clipping
					   counts, respectively   */
					if ((pfBuffer[k + offset] == fMax) || (iMaxFound && (pfBuffer[j=k + offset] > .999))) {
						if (iMaxFound) {
							iClips++;
							iTotalClips++;
							iFrameClips++;
							//printf("Clipped: %d\n", piBuffer[j]);
						} else {
							iMaxFound++;
						}
					} else if ((pfBuffer[k + offset] == fMin) || (iMinFound && (pfBuffer[k + offset] < -.999))) {
						if (iMinFound) {
							iClips++;
							iTotalClips++;
							iFrameClips++;
						} else {
							iMinFound++;
						}
					} else {
						iMaxFound = 0;
						iMinFound = 0;
						iClips = 0;
					}
					
					
		
				}
				/* The if statements below will catch the ends of each clip event
				   and print to the terminal which event was encountered and some 
				   details about the event  */
				fPercentClipped = (float) iFrameClips / ((float) iFrameLength);
			
				if (fPercentClipped > fMCBThreshold) {
					// Major Clip Block
					printf("Frame %d has %.2f percent of clipped samples - MCB\n", i, fPercentClipped * 100);
					iTotalMCB++;
				} else if (fPercentClipped > 0) {
					// Isolated Clip Event
					printf("Frame %d has %.2f percent of clipped samples - ICE\n", i, fPercentClipped * 100);
					iTotalICE++;
				}
				// else, there was no clipping present in this frame
			
			
			
				iMaxFound = 0;
				iMinFound = 0;	
				iClips = 0;
				iFrameClips = 0;
				
			//}	
			
		}
		
		// Print summary
		if ((iTotalMCB == 0) && (iTotalICE == 0)) {
			printf("No clipping.\n");
		} else {
			printf("There were %d frames with MCBs, and %d frames with ICEs.\n", iTotalMCB, iTotalICE);
		}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	} else {
		/********************   Sequential-Sample Based Processing   ********************/	
		
		/**
		/*  This algorithm detects clipping by looking for a single sample that is 
		/*  equal to the maximum sample found in the entire audio file, followed
		/*  by one or more samples that are equal to the maximum sample minus a percentage (alpha)
		/*  of that maximum sample value.  This is to account for a/d converters that do not
		/*  consistently produce the maximum possible value once the input has been saturated.
		/*  Using this method has proved to be much more accurate than only considering a string
		/*  of samples clipped if they all reach the maximum value.  A reasonable value for alpha 
		/*  determined by hand examining clipped audio files and the output of the algorithm 
		/*  is 0.5% (.005)       
		/*															*/
		
		// Use the sample rate to calculate the number of clipped
		// samples in a row necessary for a major clip block
		iBlockSamples = iSampleRate * fBlockLength;
		
		fLog = fopen(argv[2],"a+"); /* apend file (add text to a file or create a file if it does not exist.*/ 
		// printf("fBlockLength = %f, iBlockSamples = %d\n", fBlockLength, iBlockSamples);
		
		/* For each channel  */
		// NOTE!!! 
		//    This file was modified to only do ONE channel from the input audio file
		//    This is because the NIST lists specify A or B as the channel of interest
		//    Instead of looping through each channel here, j is assigned to the desired
		//    channel, according to the input flags.  Defaults to channel 0.
		i = iStartChannel;
		//for (i = 0; i < iChannels; i++) {
	
			/* Look at each sample for that channel  */
			for (j = i; (j+iChannels) < iReadCount; j += iChannels) {
				/* Note: index = j;
				/*   If the maximum value is found -OR-
				/*	   it has previously been found and current
				/*	   sample is over threshold (currently .999),
				/*   Set flag that max has been found, or update clipping
				/*   counts, respectively   */
				if ((pfBuffer[j] == fMax) || (iMaxFound && (pfBuffer[j] > .999))) {
					if (iMaxFound) {
						iClips++;
						iTotalClips++;
						//printf("Clipped: %d\n", piBuffer[j]);
						if (fStartTime == 0) {
							fStartTime = (float) j / 8000.0;
						}
					} else {
						iMaxFound++;
						// iMinFound = 0; //?
					}
				} else if ((pfBuffer[j] == fMin) || (iMinFound && (pfBuffer[j] < -.999))) {
					if (iMinFound) {
						iClips++;
						iTotalClips++;
						//printf("Clipped: %d\n", piBuffer[j]);
						if (fStartTime == 0) {
							fStartTime = (float) j / 8000.0;
						}
					} else {
						iMinFound++;
						// iMaxFound = 0; //?
					}
				} else {
					// The if statements below will catch the ends of each clip event
					// and print the labels to the logfile specified in the command line argument
					// which include the type of clipping event, as well as the begin and 
					// end time stamps
					if (iMaxFound && (iClips > iBlockSamples)) {
						// Major Clip Block
						fEndTime = (float) j / 8000.0;
						fprintf(fLog,"%f\t%f\tMCB\n", fStartTime, fEndTime);
						iPrint = 1;
					} else if (iMinFound && (iClips > iBlockSamples)) {
						// Major Clip Block
						fEndTime = (float) j / 8000.0;
						fprintf(fLog,"%f\t%f\tMCB\n", fStartTime, fEndTime);
						iPrint = 1;
					} else if (iMaxFound && (iClips > 0)) {
						// Isolated Clip Event
						fEndTime = (float) j / 8000.0;
						fprintf(fLog,"%f\t%f\tICE\n", fStartTime, fEndTime);
						iPrint = 1;
					} else if (iMinFound && (iClips > 0)) {
						// Isolated Clip Event
						fEndTime = (float) j / 8000.0;
						fprintf(fLog,"%f\t%f\tICE\n", fStartTime, fEndTime);
						iPrint = 1;
					} 
					
					
					
					iMaxFound = 0;
					iMinFound = 0;
					iClips = 0;
					fStartTime = 0;
					fEndTime = 0;
				}
				
			}
			iMaxFound = 0;
			iMinFound = 0;	
			iClips = 0;
			fStartTime = 0;
			fEndTime = 0;
		//}
		
		fclose(fLog);
	
	// Print summary
    printf("In total, there were %d samples clipped.\n", iTotalClips);

	}

    return 0;
}
