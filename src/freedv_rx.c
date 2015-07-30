/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: freedv_rx.c
  AUTHOR......: David Rowe
  DATE CREATED: August 2014
                                                                             
  Demo receive program for FreeDV API functions, some side information
  written to freedv_rx_log.txt
 
  Example usage (all one line):

     codec2-dev/build_linux/src$ ./freedv_tx 1600 ../../raw/ve9qrp_10s.raw - | 
                                 ./freedv_rx 1600 - - | play -t raw -r 8000 -s -2 -
                                                             
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2014 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "freedv_api.h"

struct my_callback_state {
    FILE *ftxt;
};

void my_put_next_rx_char(void *callback_state, char c) {
    struct my_callback_state* pstate = (struct my_callback_state*)callback_state;
    if (pstate->ftxt != NULL) {
        fprintf(pstate->ftxt, "%c", c);
    }
}

int main(int argc, char *argv[]) {
    FILE                      *fin, *fout, *ftxt;
    short                     *speech_out;
    short                     *demod_in;
    struct freedv             *freedv;
    int                        nin, nout, frame = 0;
    struct my_callback_state   my_cb_state;
    int                        mode;

    if (argc < 4) {
	printf("usage: %s 1600|700 InputModemSpeechFile OutputSpeechRawFile [--test_frames]\n", argv[0]);
	printf("e.g    %s 1600 hts1a_fdmdv.raw hts1a_out.raw txtLogFile\n", argv[0]);
	exit(1);
    }

    mode = -1;
    if (!strcmp(argv[1],"1600"))
        mode = FREEDV_MODE_1600;
    if (!strcmp(argv[1],"700"))
        mode = FREEDV_MODE_700;
    assert(mode != -1);

    if (strcmp(argv[2], "-")  == 0) fin = stdin;
    else if ( (fin = fopen(argv[2],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input raw modem sample file: %s: %s.\n",
         argv[2], strerror(errno));
	exit(1);
    }

    if (strcmp(argv[3], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[3],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output speech sample file: %s: %s.\n",
         argv[3], strerror(errno));
	exit(1);
    }
    
    freedv = freedv_open(mode);
    assert(freedv != NULL);

    if ( (argc > 4) && (strcmp(argv[4], "--testframes") == 0) ) {
        freedv->test_frames = 1;
    }
    
    speech_out = (short*)malloc(sizeof(short)*freedv->n_speech_samples);
    assert(speech_out != NULL);
    demod_in = (short*)malloc(sizeof(short)*freedv->n_max_modem_samples);
    assert(demod_in != NULL);

    ftxt = fopen("freedv_rx_log.txt","wt");
    assert(ftxt != NULL);
    my_cb_state.ftxt = ftxt;
    freedv->callback_state = (void*)&my_cb_state;
    freedv->freedv_put_next_rx_char = &my_put_next_rx_char;

    freedv->snr_squelch_thresh = -100.0;

    /* Note we need to work out how many samples demod needs on each
       call (nin).  This is used to adjust for differences in the tx and rx
       sample clock frequencies.  Note also the number of output
       speech samples is time varying (nout). */

    nin = freedv_nin(freedv);
    while(fread(demod_in, sizeof(short), nin, fin) == nin) {
        frame++;

        nout = freedv_rx(freedv, speech_out, demod_in);
        nin = freedv_nin(freedv);

        fwrite(speech_out, sizeof(short), nout, fout);
        
        /* log some side info to the txt file */
               
        if (ftxt != NULL) {
            fprintf(ftxt, "frame: %d  demod sync: %d  demod snr: %3.2f dB  bit errors: %d\n", frame, 
                    freedv->sync, freedv->snr_est, freedv->total_bit_errors);
        }

	/* if this is in a pipeline, we probably don't want the usual
           buffering to occur */

        if (fout == stdout) fflush(stdout);
        if (fin == stdin) fflush(stdin);         
    }

    if (freedv->test_frames) {
        fprintf(stderr, "bits: %d errors: %d BER: %3.2f\n", freedv->total_bits, freedv->total_bit_errors, (float)freedv->total_bit_errors/freedv->total_bits);
    }

    free(speech_out);
    free(demod_in);
    freedv_close(freedv);
    fclose(fin);
    fclose(fout);

    return 0;
}

