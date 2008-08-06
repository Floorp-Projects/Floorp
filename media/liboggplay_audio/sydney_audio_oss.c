/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is
 * CSIRO
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Marcin Lubonski 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** *
 */

#include "sydney_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/soundcard.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define SA_READ_PERIOD 0 
#define SA_WRITE_PERIOD 2560 // 40 ms of 16-bit, stereo, 16kHz
#define SA_READ_BUFFER 0
#define SA_WRITE_BUFFER 7680 // 3 periods per buffer

// for versions newer than 3.6.1
#define OSS_VERSION(x, y, z) (x << 16 | y << 8 | z)
// support only versions newer than 3.6.1
#define SUPP_OSS_VERSION OSS_VERSION(3,6,1)

#if (SOUND_VERSION >= SUPP_OSS_VERSION)

struct SAAudioHandle_ {
   char *device_name;
   int channels;
   int read_period;
   int write_period;
   int read_buffer;
   int write_buffer;
   sa_pcm_mode_t rw_mode;
   sa_pcm_format_t format;
   int rate;
   int interleaved;

   int capture_handle;
   int playback_handle;
   int readN, writeN;
   char *stored;
   int stored_amount;
   int stored_limit;
   //int read_fd, write_fd;
};

/* Implemented API functions */
/** Normal way to open PCM device */
int sa_device_create_pcm(SAAudioHandle **_dev, const char *client_name, sa_pcm_mode_t rw_mode, sa_pcm_format_t format, int rate, int channels);
/** Initialise the device */
int sa_device_open(SAAudioHandle *dev);
/** Close/destroy everything */
int sa_device_close(SAAudioHandle *dev);

/* Soft parameter setup - can only be called before calling open*/
/** Set write buffer lower mark */
int sa_device_set_write_lower_watermark(SAAudioHandle *dev, int size);
/** Set read buffer lower mark */
int sa_device_set_read_lower_watermark(SAAudioHandle *dev, int size);
/** Set write buffer upper watermark */
int sa_device_set_write_upper_watermark(SAAudioHandle *dev, int size);
/** Set read buffer upper watermark */
int sa_device_set_read_upper_watermark(SAAudioHandle *dev, int size);

/** volume in hundreths of dB's*/
int sa_device_change_input_volume(SAAudioHandle *dev, const int *vol);
/** volume in hundreths of dB's*/
int sa_device_change_output_volume(SAAudioHandle *dev, const int *vol);

/** Read audio playback position */
int sa_device_get_position(SAAudioHandle *dev, sa_pcm_index_t ref, int64_t *pos);

/* Blocking I/O calls */
/** Interleaved playback function */
int sa_device_write(SAAudioHandle *dev, size_t nbytes, const void *data);


/* Implementation-specific functions */
static int oss_audio_format(sa_pcm_format_t sa_format, int* oss_format);
//static void sa_print_handle_settings(SAAudioHandle* dev);

/*!
 * \brief fills in the SAAudioHandle struct
 * \param SAAudioHandle - encapsulation of a handle to audio device
 * \param client_name - 
 * \param rw_mode - requested device access type as in :: sa_pcm_mode_t
 * \param format - audio format as specified in ::sa_pcm_format_t
 * \return - Sydney API error as in ::sa_pcm_error_t 
 */
int sa_device_create_pcm(SAAudioHandle **_dev, const char *client_name, sa_pcm_mode_t rw_mode, sa_pcm_format_t format, int rate, int channels) {
	SAAudioHandle* dev = NULL;
	
	dev = malloc(sizeof(SAAudioHandle));
	
	if (!dev) {
		return SA_DEVICE_OOM;	
	}		        
	dev->channels = channels;
	dev->format = format;
	dev->rw_mode = rw_mode;
	dev->rate = rate;
	dev->readN = 0;
	dev->readN = 0;
	dev->capture_handle = -1;
	dev->playback_handle = -1;
	dev->interleaved = 1;
	dev->read_period = SA_READ_PERIOD;
	dev->write_period = SA_WRITE_PERIOD;
	dev->read_buffer = SA_READ_BUFFER;
	dev->write_buffer = SA_WRITE_BUFFER;
  dev->device_name = "/dev/dsp";
  dev->stored = NULL;
  dev->stored_amount = 0;
  dev->stored_limit = 0;
	
	*_dev = dev;
	 //sa_print_handle_settings(dev);
	 return SA_DEVICE_SUCCESS;
}

/*!
 * \brief creates and opens selected audio device
 * \param dev - encapsulated audio device handle
 * \return - Sydney API error as in ::sa_pcm_error_t  
 */
int sa_device_open(SAAudioHandle *dev) {	
 	int err;
	int fmt;
	int audio_fd = -1;

	if (dev->rw_mode == SA_PCM_WRONLY) {
		// open the default OSS device
		dev->device_name = "/dev/dsp"; // replace with a function which returns audio ouput device best matching the settings
		audio_fd = open(dev->device_name, O_WRONLY, 0);
		if (audio_fd == -1) {
		   fprintf(stderr, "Cannot open device: %s\n", dev->device_name);
		   //assert(0);
		   return SA_DEVICE_OOM;
		}

		// set the playback rate
		if ((err = ioctl(audio_fd, SNDCTL_DSP_SPEED, &(dev->rate))) < 0) {
		   fprintf(stderr, 
			"Error setting the audio playback rate [%d]\n", 
			dev->rate);
		   //assert(0);
		   return SA_DEVICE_OOM; 
		}
		// set the channel numbers
		if ((err = ioctl(audio_fd,  SNDCTL_DSP_CHANNELS, 
			&(dev->channels)))< 0) {
		   fprintf(stderr, "Error setting audio channels\n");
		   //assert(0);
		   return SA_DEVICE_OOM;	
		} 
		if ((err = oss_audio_format(dev->format, &fmt)) < 0) {
		   fprintf(stderr, "Format unknown\n");
		   //assert(0);
                   return SA_DEVICE_OOM;	   
		}
		printf("Setting format with value %d\n", fmt);
		if ((err = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &fmt)) < 0 ) {
		   fprintf(stderr, "Error setting audio format\n");
		   //assert(0);
		   return SA_DEVICE_OOM;
		}

		dev->playback_handle = audio_fd;
 		
	}
	if (dev->rw_mode == SA_PCM_RDONLY) {
		return SA_DEVICE_NOT_SUPPORTED;
	} 
	if (dev->rw_mode == SA_PCM_RW) {
		return SA_DEVICE_NOT_SUPPORTED;
	}
	fprintf(stderr, "Audio device opened successfully\n");
	return SA_DEVICE_SUCCESS;
}

#define WRITE(data,amt)                                       \
  if ((err = write(dev->playback_handle, data, amt)) < 0) {   \
    fprintf(stderr, "Error writing data to audio device\n");  \
    return SA_DEVICE_OOM;                                     \
  }

/*!
 * \brief Interleaved write operation
 * \param dev - device handle
 * \param nbytes - size of the audio buffer to be written to device
 * \param data - pointer to the buffer with audio samples
 * \return 
 * */
int sa_device_write(SAAudioHandle *dev, size_t nbytes, const void *_data) {
	int               err;
  audio_buf_info    info;
  int               bytes;
  char            * data = (char *)_data;
  /*
  ioctl(dev->playback_handle, SNDCTL_DSP_GETOSPACE, &info);
  printf("fragment size: %d, nfrags: %d, free: %d wtw: %d\n", info.fragsize, 
                  info.fragstotal, info.bytes, nbytes);
  */



	if ((dev->playback_handle) > 0) {
    ioctl(dev->playback_handle, SNDCTL_DSP_GETOSPACE, &info);
    bytes = info.bytes;
    if (dev->stored_amount > bytes) {
      WRITE(dev->stored, bytes);
      memmove(dev->stored, dev->stored + bytes, dev->stored_amount - bytes);
      dev->stored_amount -= bytes;
    } else if (dev->stored_amount > 0) {
      WRITE(dev->stored, dev->stored_amount);
      bytes -= dev->stored_amount;
      dev->stored_amount = 0;
      if (nbytes < bytes) {
        WRITE(data, nbytes);
        return SA_DEVICE_SUCCESS;
      }
      WRITE(data, bytes);
      data += bytes;
      nbytes -= bytes;
    } else {
      if (nbytes < bytes) {
        WRITE(data, nbytes);
        return SA_DEVICE_SUCCESS;
      }
      WRITE(data, bytes);
      data += bytes;
      nbytes -= bytes;
    }

    if (nbytes > 0) {
      if (dev->stored_amount + nbytes > dev->stored_limit) {
        dev->stored = realloc(dev->stored, dev->stored_amount + nbytes);
      }
      
      memcpy(dev->stored + dev->stored_amount, data, nbytes);
      dev->stored_amount += nbytes;
    }
	}
	return SA_DEVICE_SUCCESS;
}

#define CLOSE_HANDLE(x) if (x != -1) close(x);

/*!
 * \brief Closes and destroys allocated resources
 * \param dev - Sydney Audio device handle
 * \return Sydney API error as in ::sa_pcm_error_t
 **/
int sa_device_close(SAAudioHandle *dev) {
  int err;

	if (dev != NULL) {

    if (dev->stored_amount > 0) {
      WRITE(dev->stored, dev->stored_amount);
    }

    if (dev->stored != NULL) {
      free(dev->stored);
    }

    dev->stored = NULL;
    dev->stored_amount = 0;
    dev->stored_limit = 0;
	  
    CLOSE_HANDLE(dev->playback_handle);
	  CLOSE_HANDLE(dev->capture_handle);

	  printf("Closing audio device\n");	
	  free(dev);
	}
 	return SA_DEVICE_SUCCESS;
}

/** 
 * \brief 
 * \param dev
 * \param size
 * \return  Sydney API error as in ::sa_pcm_error_t
 */
int sa_device_set_write_lower_watermark(SAAudioHandle *dev, int size) {
   dev->write_period = size;
   return SA_DEVICE_SUCCESS;
}
/** 
 * \brief
 * \param dev
 * \param size
 * \return  Sydney API error as in ::sa_pcm_error_t
 */
int sa_device_set_read_lower_watermark(SAAudioHandle *dev, int size) {
   dev->read_period = size;
   return SA_DEVICE_SUCCESS;
}
/** 
 * \brief
 * \param dev
 * \param size
 * \return  Sydney API error as in ::sa_pcm_error_t
 */
int sa_device_set_write_upper_watermark(SAAudioHandle *dev, int size) {  
   dev->write_buffer = size;
   return SA_DEVICE_SUCCESS;
}

/** 
 * \brief
 * \param dev
 * \param size
 * \return  Sydney API error as in ::sa_pcm_error_t
 */
int sa_device_set_read_upper_watermark(SAAudioHandle *dev, int size) {
   dev->read_buffer = size;
   return SA_DEVICE_SUCCESS;
}


int sa_device_set_xrun_mode(SAAudioHandle *dev, sa_xrun_mode_t mode) {
   return SA_DEVICE_NOT_SUPPORTED;
}


int sa_device_set_ni(SAAudioHandle *dev) {
   dev->interleaved = 1;
   return SA_DEVICE_SUCCESS;
}

int sa_device_start_thread(SAAudioHandle *dev, sa_device_callback *callback) {
   return SA_DEVICE_NOT_SUPPORTED;
}

int sa_device_set_channel_map(SAAudioHandle *dev, const sa_channel_def_t *map) {
   return SA_DEVICE_NOT_SUPPORTED;
}


int sa_device_change_device(SAAudioHandle *dev, const char *device_name) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/*!
 * \brief volume in hundreths of dB's
 * \param dev - device handle
 * \param vol - volume level 
 * \return Sydney API error as in ::sa_pcm_error_t
 * */
int sa_device_change_input_volume(SAAudioHandle *dev, const int *vol) {
#if SOUND_VERSION >= OSS_VERSION(4,0,0)	
	int err;
	if ((err = ioctl(dev->capture_handle, SNDCTL_DSP_SETRECVOL, vol) < 0) {
	   fpritnf(stderr, "Error setting new recording volume level\n");
	   //assert(0);
           return SA_DEVICE_OOM;	   
	}
	return SA_DEVICE_SUCCESS;
#else
	return SA_DEVICE_NOT_SUPPORTED;
#endif
}

/*!
 * \brief volume in hundreths of dB's
 * \param dev - device handle
 * \param vol - volume level
 * \retrun  Sydney API error as in ::sa_pcm_error_t
 * */
int sa_device_change_output_volume(SAAudioHandle *dev, const int *vol) {
#if SOUND_VERSION >= OSS_VERSION(4,0,0)
	int err;
	if ((err = ioctl(dev->playback_handle, SNDCTL_DSP_SETPLAYVOL, vol) < 0){

	fprintf(stderr, "Error setting new playback volume\n");
	//assert(0);
	return SA_DEVICE_OOM; 
        }
	return SA_DEVICE_SUCCESS;
#else
	return SA_DEVICE_NOT_SUPPORTED;
#endif
}

int sa_device_change_sampling_rate(SAAudioHandle *dev, int rate) {
   dev->rate = rate;
   return SA_DEVICE_SUCCESS;
}

int sa_device_change_client_name(SAAudioHandle *dev, const char *client_name) {
   return SA_DEVICE_NOT_SUPPORTED;
}

int sa_device_change_stream_name(SAAudioHandle *dev, const char *stream_name) {
   return SA_DEVICE_NOT_SUPPORTED;
}

int sa_device_change_user_data(SAAudioHandle *dev, void *val) {
   return SA_DEVICE_NOT_SUPPORTED;
}


/*!
 * \brief
 * \param dev
 * \param rate
 * \param direction
 * \return  Sydney API error as in ::sa_pcm_error_t
 * */
int sa_device_adjust_rate(SAAudioHandle *dev, int rate, int direction) {
   return  SA_DEVICE_NOT_SUPPORTED;
}
/*!
 * \brief
 * \param dev
 * \param nb_channels
 * \return  Sydney API error as in ::sa_pcm_error_t
 * */
int sa_device_adjust_channels(SAAudioHandle *dev, int nb_channels) {               return SA_DEVICE_NOT_SUPPORTED;
}
/** Adjust bit sample format */
int sa_device_adjust_format(SAAudioHandle *dev, sa_pcm_format_t format, int direction) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Get current state of the audio device */
int sa_device_get_state(SAAudioHandle *dev, sa_state_t *running) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Get current sampling rate */
int sa_device_get_sampling_rate(SAAudioHandle *dev, int *rate) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Get number of channels */
int sa_device_get_nb_channels(SAAudioHandle *dev, int *nb_channels) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Get format being used */
int sa_device_get_format(SAAudioHandle *dev, sa_pcm_format_t *format) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Get opaque pointer associated to the device */
int sa_device_get_user_data(SAAudioHandle *dev, void **val) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Obtain the error code */
int sa_device_get_event_error(SAAudioHandle *dev, sa_pcm_error_t *error) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/** Obtain the notification code */
int sa_device_get_event_notify(SAAudioHandle *dev, sa_pcm_notification_t *notify) {
   return SA_DEVICE_NOT_SUPPORTED;
}

/*!
 * \brief returns the current position of the audio playback capture
 * \param dev - device handle
 * \param ref - type of position to be returned by this function see ::sa_pcm_index_t
 * \param pos - position (in bytes or ms depending on 'ref' value)
 * \return  Sydney API error as in ::sa_pcm_error_t
 * */
int sa_device_get_position(SAAudioHandle *dev, sa_pcm_index_t ref, int64_t *pos)
{
   int err;
   int64_t _pos;
   int delay;
   count_info ptr;
   switch (ref) {
	 case SA_PCM_WRITE_DELAY:
	      //int delay;
	      if ((err = ioctl(dev->playback_handle, 
			       SNDCTL_DSP_GETODELAY, 
 			       &delay)) <0) {
	      	fprintf(stderr, "Error reading playback buffering delay\n");
		return SA_DEVICE_OOM;	
	      };  
	      _pos = (int64_t)delay;
	      break;
         case SA_PCM_WRITE_SOFTWARE_POS:
              //count_info ptr;
	      if ((err = ioctl(dev->playback_handle, 
                               SNDCTL_DSP_GETOPTR, 
                               &ptr)) <0) {
                //fprintf(stderr, "Error reading audio playback position\n");
		return SA_DEVICE_OOM;
              };
	      _pos = (int64_t)ptr.bytes;  
	      break;
         case SA_PCM_READ_SOFTWARE_POS:
              //count_info ptr;
	      if ((err = ioctl(dev->playback_handle, 
                               SNDCTL_DSP_GETIPTR, 
                               &ptr)) <0) {
              	 fprintf(stderr, "Error reading audio capture position\n");
		 return SA_DEVICE_OOM;
	      };
               _pos = (int64_t)ptr.bytes;
	      break;

	 case SA_PCM_READ_DELAY:
	 case SA_PCM_READ_HARDWARE_POS:
	 case SA_PCM_WRITE_HARDWARE_POS:               
	 case SA_PCM_DUPLEX_DELAY:
	 default:
	      return SA_DEVICE_NOT_SUPPORTED;
	      break;		
   }
   (*pos) = _pos;
   return SA_DEVICE_SUCCESS;
}

/** Private functions - implementation specific */

/*!
 * \brief private function mapping Sudney Audio format to OSS formats
 * \param format - Sydney Audio API specific format
 * \param - filled by the function with a value for corresponding OSS format
 * \return - Sydney API error value as in ::sa_pcm_format_t
 * */
static int oss_audio_format(sa_pcm_format_t sa_format, int* oss_format) {
#if SOUND_VERSION >= OSS_VERSION(4,0,0) 	
	int fmt = AFMT_UNDEF;
#else
	int fmt = -1;
#endif	
	switch (sa_format) {
                   case SA_PCM_UINT8:
			fmt = AFMT_U8;
			break;
                   case SA_PCM_ULAW:
			fmt = AFMT_MU_LAW;
			break;
                   case SA_PCM_ALAW:
			fmt = AFMT_A_LAW;
			break;
		   /* 16-bit little endian (LE) format */
                   case SA_PCM_S16_LE:
			fmt = AFMT_S16_LE;
			break;
		   /* 16-bit big endian (BE) format */
                   case SA_PCM_S16_BE:
			fmt = AFMT_S16_BE;
			break;
#if SOUND_VERSION >= OSS_VERSION(4,0,0)
		   /* 24-bit formats (LSB aligned in 32 bit word) */
                   case SA_PCM_S24_LE:
			fmt = AFMT_S24_LE;
			break;
		   /* 24-bit formats (LSB aligned in 32 bit word) */
		   case SA_PCM_S24_BE:
			fmt = AFMT_S24_BE;
			break;
		   /* 32-bit format little endian */
                   case SA_PCM_S32_LE:
			fmt = AFMT_S32_LE;
			break;
		   /* 32-bit format big endian */
                   case SA_PCM_S32_BE:
			fmt = AFMT_S32_BE;
			break; 
                   case SA_PCM_FLOAT32_NE:
			fmt = AFMT_FLOAT;
			break;
#endif
		   default:
			return SA_DEVICE_NOT_SUPPORTED;
			break;

                }
	(*oss_format) = fmt;
	return SA_DEVICE_SUCCESS;
}

/*
static void sa_print_handle_settings(SAAudioHandle* dev) {
    printf(">>>>>>>>>>>> SA Device Handle <<<<<<<<<<<\n"); 
    printf("[SA Audio] - Device name %s\n", dev->device_name);
    printf("[SA_Audio] - Number of audio channels %d\n", dev->channels);
    printf("[SA_Audio] - Read period size %d bytes\n", dev->read_period);
    printf("[SA_Audio] - Write period size %d bytes\n", dev->write_period);
    printf("[SA_Audio] - Write buffer size %d bytes\n", dev->write_buffer);
    printf("[SA_Audio] - Read buffer size %d bytes\n", dev->read_buffer);
    printf("[SA_Audio] - Read/write mode value %d\n", dev->rw_mode);
    printf("[SA_Audio] - Audio sample bit format value %d\n", dev->format);
    printf("[SA_Audio] - Audio playback rate %d\n", dev->rate);
    if (dev->interleaved) {
        printf("[SA_Audio] - Processing interleaved audio\n");
    } else {
	printf("[SA_Audio] - Processing non-interleaved audio\n");
    }
    if ((dev->capture_handle) > 0) {
    	printf("[SA Audio] - Device opened for capture\n");
    } 
    if ((dev->playback_handle) > 0) {
    	printf("[SA_Audio] - Device opened for playback\n");
    }	    
}
*/

#endif // (SOUND_VERSION > SUPP_OSS_VERSION)
