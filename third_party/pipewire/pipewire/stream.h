/* PipeWire
 *
 * Copyright © 2018 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef PIPEWIRE_STREAM_H
#define PIPEWIRE_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page page_streams Media Streams
 *
 * \section sec_overview Overview
 *
 * Media streams are used to exchange data with the PipeWire server. A
 * stream is a wrapper around a proxy for a \ref pw_client_node with
 * an adapter. This means the stream will automatically do conversion
 * to the type required by the server.
 *
 * Streams can be used to:
 *
 * \li Consume a stream from PipeWire. This is a PW_DIRECTION_INPUT stream.
 * \li Produce a stream to PipeWire. This is a PW_DIRECTION_OUTPUT stream
 *
 * You can connect the stream port to a specific server port or let PipeWire
 * choose a port for you.
 *
 * For more complicated nodes such as filters or ports with multiple
 * inputs and/or outputs you will need to use the pw_filter or make
 * a pw_node yourself and export it with \ref pw_core_export.
 *
 * \section sec_create Create
 *
 * Make a new stream with \ref pw_stream_new(). You will need to specify
 * a name for the stream and extra properties. You can use \ref
 * pw_fill_stream_properties() to get a basic set of properties for the
 * stream.
 *
 * Once the stream is created, the state_changed event should be used to
 * track the state of the stream.
 *
 * \section sec_connect Connect
 *
 * The stream is initially unconnected. To connect the stream, use
 * \ref pw_stream_connect(). Pass the desired direction as an argument.
 *
 * \subsection ssec_stream_target Stream target
 *
 * To make the newly connected stream automatically connect to an existing
 * PipeWire node, use the \ref PW_STREAM_FLAG_AUTOCONNECT and the port_path
 * argument while connecting.
 *
 * \subsection ssec_stream_formats Stream formats
 *
 * An array of possible formats that this stream can consume or provide
 * must be specified.
 *
 * \section sec_format Format negotiation
 *
 * After connecting the stream, the server will want to configure some
 * parameters on the stream. You will be notified of these changes
 * with the param_changed event.
 *
 * When a format param change is emitted, the client should now prepare
 * itself to deal with the format and complete the negotiation procedure
 * with a call to \ref pw_stream_update_params().
 *
 * As arguments to \ref pw_stream_update_params() an array of spa_param
 * structures must be given. They contain parameters such as buffer size,
 * number of buffers, required metadata and other parameters for the
 * media buffers.
 *
 * \section sec_buffers Buffer negotiation
 *
 * After completing the format negotiation, PipeWire will allocate and
 * notify the stream of the buffers that will be used to exchange data
 * between client and server.
 *
 * With the add_buffer event, a stream will be notified of a new buffer
 * that can be used for data transport. You can attach user_data to these
 * buffers.
 *
 * After the buffers are negotiated, the stream will transition to the
 * \ref PW_STREAM_STATE_PAUSED state.
 *
 * \section sec_streaming Streaming
 *
 * From the \ref PW_STREAM_STATE_PAUSED state, the stream can be set to
 * the \ref PW_STREAM_STATE_STREAMING state by the PipeWire server when
 * data transport is started.
 *
 * Depending on how the stream was connected it will need to Produce or
 * Consume data for/from PipeWire as explained in the following
 * subsections.
 *
 * \subsection ssec_consume Consume data
 *
 * The process event is emitted for each new buffer that can can be
 * consumed.
 *
 * \ref pw_stream_dequeue_buffer() should be used to get the data and
 * metadata of the buffer.
 *
 * When the buffer is no longer in use, call \ref pw_stream_queue_buffer()
 * to let PipeWire reuse the buffer.
 *
 * \subsection ssec_produce Produce data
 *
 * \ref pw_stream_dequeue_buffer() gives an empty buffer that can be filled.
 *
 * Filled buffers should be queued with \ref pw_stream_queue_buffer().
 *
 * The process event is emitted when PipeWire has emptied a buffer that
 * can now be refilled.
 *
 * \section sec_stream_disconnect Disconnect
 *
 * Use \ref pw_stream_disconnect() to disconnect a stream after use.
 */
/** \class pw_stream
 *
 * \brief PipeWire stream object class
 *
 * The stream object provides a convenient way to send and
 * receive data streams from/to PipeWire.
 *
 * See also \ref page_streams and \ref page_context_api
 */
struct pw_stream;

#include <spa/buffer/buffer.h>
#include <spa/param/param.h>

/** \enum pw_stream_state The state of a stream \memberof pw_stream */
enum pw_stream_state {
	PW_STREAM_STATE_ERROR = -1,		/**< the stream is in error */
	PW_STREAM_STATE_UNCONNECTED = 0,	/**< unconnected */
	PW_STREAM_STATE_CONNECTING = 1,		/**< connection is in progress */
	PW_STREAM_STATE_PAUSED = 2,		/**< paused */
	PW_STREAM_STATE_STREAMING = 3		/**< streaming */
};

struct pw_buffer {
	struct spa_buffer *buffer;	/**< the spa buffer */
	void *user_data;		/**< user data attached to the buffer */
	uint64_t size;			/**< This field is set by the user and the sum of
					  *  all queued buffer is returned in the time info */
};

struct pw_stream_control {
	const char *name;		/**< name of the control */
	uint32_t flags;			/**< extra flags (unused) */
	float def;			/**< default value */
	float min;			/**< min value */
	float max;			/**< max value */
	float *values;			/**< array of values */
	uint32_t n_values;		/**< number of values in array */
	uint32_t max_values;		/**< max values that can be set on this control */
};

/** A time structure \memberof pw_stream */
struct pw_time {
	int64_t now;			/**< the monotonic time */
	struct spa_fraction rate;	/**< the rate of \a ticks and delay */
	uint64_t ticks;			/**< the ticks at \a now. This is the current time that
					  *  the remote end is reading/writing. */
	int64_t delay;			/**< delay to device, add to ticks to get the time of the
					  *  device. Positive for INPUT streams and
					  *  negative for OUTPUT streams. */
	uint64_t queued;		/**< data queued in the stream, this is the sum
					  *  of the size fields in the pw_buffer that are
					  *  currently queued */
};

#include <pipewire/pipewire.h>

/** Events for a stream. These events are always called from the mainloop
 * unless explicitly documented otherwise. */
struct pw_stream_events {
#define PW_VERSION_STREAM_EVENTS	0
	uint32_t version;

	void (*destroy) (void *data);
	/** when the stream state changes */
	void (*state_changed) (void *data, enum pw_stream_state old,
				enum pw_stream_state state, const char *error);

	/** Notify information about a control.  */
	void (*control_info) (void *data, uint32_t id, const struct pw_stream_control *control);

	/** when io changed on the stream. */
	void (*io_changed) (void *data, uint32_t id, void *area, uint32_t size);
	/** when a parameter changed */
	void (*param_changed) (void *data, uint32_t id, const struct spa_pod *param);

        /** when a new buffer was created for this stream */
        void (*add_buffer) (void *data, struct pw_buffer *buffer);
        /** when a buffer was destroyed for this stream */
        void (*remove_buffer) (void *data, struct pw_buffer *buffer);

        /** when a buffer can be queued (for playback streams) or
         *  dequeued (for capture streams). This is normally called from the
	 *  mainloop but can also be called directly from the realtime data
	 *  thread if the user is prepared to deal with this. */
        void (*process) (void *data);

	/** The stream is drained */
        void (*drained) (void *data);

};

/** Convert a stream state to a readable string \memberof pw_stream */
const char * pw_stream_state_as_string(enum pw_stream_state state);

/** \enum pw_stream_flags Extra flags that can be used in \ref pw_stream_connect() \memberof pw_stream */
enum pw_stream_flags {
	PW_STREAM_FLAG_NONE = 0,			/**< no flags */
	PW_STREAM_FLAG_AUTOCONNECT	= (1 << 0),	/**< try to automatically connect
							  *  this stream */
	PW_STREAM_FLAG_INACTIVE		= (1 << 1),	/**< start the stream inactive,
							  *  pw_stream_set_active() needs to be
							  *  called explicitly */
	PW_STREAM_FLAG_MAP_BUFFERS	= (1 << 2),	/**< mmap the buffers */
	PW_STREAM_FLAG_DRIVER		= (1 << 3),	/**< be a driver */
	PW_STREAM_FLAG_RT_PROCESS	= (1 << 4),	/**< call process from the realtime
							  *  thread. You MUST use RT safe functions
							  *  in the process callback. */
	PW_STREAM_FLAG_NO_CONVERT	= (1 << 5),	/**< don't convert format */
	PW_STREAM_FLAG_EXCLUSIVE	= (1 << 6),	/**< require exclusive access to the
							  *  device */
	PW_STREAM_FLAG_DONT_RECONNECT	= (1 << 7),	/**< don't try to reconnect this stream
							  *  when the sink/source is removed */
	PW_STREAM_FLAG_ALLOC_BUFFERS	= (1 << 8),	/**< the application will allocate buffer
							  *  memory. In the add_buffer event, the
							  *  data of the buffer should be set */
};

/** Create a new unconneced \ref pw_stream \memberof pw_stream
 * \return a newly allocated \ref pw_stream */
struct pw_stream *
pw_stream_new(struct pw_core *core,		/**< a \ref pw_core */
	      const char *name,			/**< a stream media name */
	      struct pw_properties *props	/**< stream properties, ownership is taken */);

struct pw_stream *
pw_stream_new_simple(struct pw_loop *loop,	/**< a \ref pw_loop to use */
		     const char *name,		/**< a stream media name */
		     struct pw_properties *props,/**< stream properties, ownership is taken */
		     const struct pw_stream_events *events,	/**< stream events */
		     void *data					/**< data passed to events */);

/** Destroy a stream \memberof pw_stream */
void pw_stream_destroy(struct pw_stream *stream);

void pw_stream_add_listener(struct pw_stream *stream,
			    struct spa_hook *listener,
			    const struct pw_stream_events *events,
			    void *data);

enum pw_stream_state pw_stream_get_state(struct pw_stream *stream, const char **error);

const char *pw_stream_get_name(struct pw_stream *stream);

struct pw_core *pw_stream_get_core(struct pw_stream *stream);

const struct pw_properties *pw_stream_get_properties(struct pw_stream *stream);

int pw_stream_update_properties(struct pw_stream *stream, const struct spa_dict *dict);

/** Connect a stream for input or output on \a port_path. \memberof pw_stream
 * \return 0 on success < 0 on error.
 *
 * You should connect to the process event and use pw_stream_dequeue_buffer()
 * to get the latest metadata and data. */
int
pw_stream_connect(struct pw_stream *stream,		/**< a \ref pw_stream */
		  enum pw_direction direction,		/**< the stream direction */
		  uint32_t target_id,			/**< the target object id to connect to or
							  *  PW_ID_ANY to let the manager
							  *  select a target. */
		  enum pw_stream_flags flags,		/**< stream flags */
		  const struct spa_pod **params,	/**< an array with params. The params
							  *  should ideally contain supported
							  *  formats. */
		  uint32_t n_params			/**< number of items in \a params */);

/** Get the node ID of the stream. \memberof pw_stream
 * \return node ID. */
uint32_t
pw_stream_get_node_id(struct pw_stream *stream);

/** Disconnect \a stream \memberof pw_stream */
int pw_stream_disconnect(struct pw_stream *stream);

/** Set the stream in error state */
int pw_stream_set_error(struct pw_stream *stream,	/**< a \ref pw_stream */
			int res,			/**< a result code */
			const char *error, ...		/**< an error message */) SPA_PRINTF_FUNC(3, 4);

/** Complete the negotiation process with result code \a res \memberof pw_stream
 *
 * This function should be called after notification of the format.

 * When \a res indicates success, \a params contain the parameters for the
 * allocation state.  */
int
pw_stream_update_params(struct pw_stream *stream,	/**< a \ref pw_stream */
			const struct spa_pod **params,	/**< an array of params. The params should
							  *  ideally contain parameters for doing
							  *  buffer allocation. */
			uint32_t n_params		/**< number of elements in \a params */);

/** Set control values */
int pw_stream_set_control(struct pw_stream *stream, uint32_t id, uint32_t n_values, float *values, ...);

/** Query the time on the stream \memberof pw_stream */
int pw_stream_get_time(struct pw_stream *stream, struct pw_time *time);

/** Get a buffer that can be filled for playback streams or consumed
 * for capture streams.  */
struct pw_buffer *pw_stream_dequeue_buffer(struct pw_stream *stream);

/** Submit a buffer for playback or recycle a buffer for capture. */
int pw_stream_queue_buffer(struct pw_stream *stream, struct pw_buffer *buffer);

/** Activate or deactivate the stream \memberof pw_stream */
int pw_stream_set_active(struct pw_stream *stream, bool active);

/** Flush a stream. When \a drain is true, the drained callback will
 * be called when all data is played or recorded */
int pw_stream_flush(struct pw_stream *stream, bool drain);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_STREAM_H */
