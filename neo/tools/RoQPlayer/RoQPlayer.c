#include <GLES/gl.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <unistd.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

#include <assert.h>

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include "../../FFmpeg/libavcodec/avcodec.h"
#include "../../FFmpeg/libavformat/avformat.h"
#include "../../FFmpeg/libavformat/avio.h"
#include "../../FFmpeg/libavutil/file.h"
#include "../../FFmpeg/libswscale/swscale.h"
#include "../../FFmpeg/libswscale/swscale_internal.h"

typedef unsigned char BYTE;

AVCodec*                                dec = NULL;
AVCodecContext*                         dec_ctx = NULL;
AVFormatContext*                        fmt_ctx = NULL;
AVFrame*                                frame = NULL;
AVFrame*                                frame2 = NULL;
SwsContext*                             img_convert_ctx = NULL;
int                                     video_stream_index = -1;
int                                     img_width = 0;
int                                     img_height = 0;
int                                     disp_width = 0;
int                                     disp_height = 0;
int                                     file_size = 0;
GLuint                                  id;

EGLDisplay                              m_display;
EGLSurface                              m_surface;
EGLContext                              m_context;
EGLConfig                               *m_config;

Display                                 *XDisplay;
BYTE                                    *image = NULL;

struct buffer_data {
    uint8_t *ptr;
    size_t size; // size left in the buffer
};

static int read_packet( void *opaque, uint8_t *buf, int buf_size )
{
    struct buffer_data *bd = ( struct buffer_data * )opaque;
    buf_size = FFMIN( buf_size, bd->size );

    //printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy( buf, bd->ptr, buf_size );
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    file_size = bd->size;
    
	return buf_size;
}

void initGL()
{
    GLuint id;
    glClearColor( 0.0, 0.0, 0.0, 0.0 );
    glColor4f( 1.0, 1.0, 1.0, 1.0 );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glGenTextures( 1, &id );

    glBindTexture( GL_TEXTURE_2D, id );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image );
}

void decodeFrame()
{
    AVPacket packet;
    int frameFinished = 0;

    // Do a single frame by getting packets until we have a full frame
    while( !frameFinished )
    {
        // if we got to the end or failed
        if( av_read_frame( fmt_ctx, &packet ) < 0 )
        {
            if( av_read_frame( fmt_ctx, &packet ) < 0 )
            {
                return;
            }
        }
        // Is this a packet from the video stream?
        if( packet.stream_index == video_stream_index )
        {
            // Decode video frame
            avcodec_decode_video2( dec_ctx, frame, &frameFinished, &packet );
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet( &packet );
    }
    // We have reached the desired frame
    // Convert the image from its native format to RGB
    sws_scale( img_convert_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, dec_ctx->height, (uint8_t * const*)frame2->data, frame2->linesize );
}

void renderGL()
{
    decodeFrame();

    glClear( GL_COLOR_BUFFER_BIT );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );

    glActiveTexture( GL_TEXTURE0 );

    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_2D, id );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image );
    float vtxcoords[] =
    {
        0, 0,
        disp_width, 0,
        0, disp_height,
        disp_width, disp_height,
    };
    float texcoords[] = { 0, 0, 1, 0, 0, 1, 1, 1 };

    glVertexPointer( 2, GL_FLOAT, 0, vtxcoords );
    glTexCoordPointer( 2, GL_FLOAT, 0, texcoords );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

    eglSwapBuffers( m_display, m_surface );

    glBindTexture( GL_TEXTURE_2D, 0 );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    glDisableClientState( GL_VERTEX_ARRAY );

}

void resizeGL( int width, int height )
{
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrthof( 0, width, height, 0, -1, +1 );
}

void makeSurface()
{
    // this code does the main window creation
    EGLBoolean result;

    Window native_window;
    EGLint totalConfigsFound = 0;

    // config you use OpenGL ES1.0 by default
    static const EGLint context_attributes[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
    };

    XSetWindowAttributes XWinAttr;
    Atom XWMDeleteMessage;
    Window XRoot;

    XDisplay = XOpenDisplay( NULL );
    if ( !XDisplay )
    {
        fprintf(stderr, "Error: failed to open X display.\n");
        return;
    }

    int screen_num = DefaultScreen( XDisplay );
    uint32_t w = DisplayWidth( XDisplay, screen_num );
    uint32_t h = DisplayHeight( XDisplay, screen_num );

    disp_width  = w;
    disp_height = h;

    XRoot = DefaultRootWindow( XDisplay );

    XWinAttr.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;

    native_window = XCreateWindow( XDisplay, XRoot, 0, 0, w, h, 0,
                                CopyFromParent, InputOutput,
                                CopyFromParent, CWEventMask, &XWinAttr );

    XWMDeleteMessage = XInternAtom( XDisplay, "WM_DELETE_WINDOW", False );

    XEvent xev;
    Atom wm_state   = XInternAtom( XDisplay, "_NET_WM_STATE", False );
    Atom fullscreen = XInternAtom( XDisplay, "_NET_WM_STATE_FULLSCREEN", False );

    memset( &xev, 0, sizeof( xev ) );
    xev.type = ClientMessage;
    xev.xclient.window = native_window;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0;
    XMapWindow( XDisplay, native_window );
    XSendEvent( XDisplay, DefaultRootWindow( XDisplay ), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev );
    XFlush( XDisplay );
    XStoreName( XDisplay, native_window, "oShaderToy" );
    XSetWMProtocols( XDisplay, native_window, &XWMDeleteMessage, 1 );

    // get an EGL display connection
    m_display = eglGetDisplay(( EGLNativeDisplayType ) XDisplay);
    if( m_display == EGL_NO_DISPLAY )
    {
        printf("error getting display\n");
        exit(EXIT_FAILURE);
    }
    // initialize the EGL display connection
    int major,minor;

    result = eglInitialize( m_display, &major, &minor );
    printf( "EGL init version %d.%d\n", major, minor );
    if( result == EGL_FALSE )
    {
        printf("error initialising display\n");
        exit( EXIT_FAILURE );
    }
    // get our config from the config class
    EGLConfig config = NULL;
    static const EGLint attribute_list[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    result = eglChooseConfig( m_display, attribute_list, &config, 1, &totalConfigsFound );
    if ( result != EGL_TRUE || totalConfigsFound == 0 )
    {
        printf( "EGLport ERROR: Unable to query for available configs, found %d.\n", totalConfigsFound );
        return;
    }

    // bind the OpenGL API to the EGL
    result = eglBindAPI( EGL_OPENGL_ES_API );
    if( result == EGL_FALSE )
    {
        printf("error binding API\n");
        exit( EXIT_FAILURE );
    }
    // create an EGL rendering context
    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, context_attributes);
    if( m_context == EGL_NO_CONTEXT )
    {
        printf( "couldn't get a valid context\n" );
        exit( EXIT_FAILURE );
    }
    // finally we can create a new surface using this config and window
    m_surface = eglCreateWindowSurface( m_display, config, (NativeWindowType)native_window, NULL );
    assert( m_surface != EGL_NO_SURFACE );
    // connect the context to the surface
    result = eglMakeCurrent( m_display, m_surface, m_surface, m_context );
    assert( EGL_FALSE != result );
}

int main(int argc, char *argv[])
{
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };

    if ( argc != 2 )
    {
        fprintf( stderr, "usage: %s input_file\n"
                "simple RoQ VideoPlayer"
                "accessed through FFmpeg's AVIOContext.\n", argv[0] );
        return 1;
    }
    input_filename = argv[1];

    /* register codecs and formats and other lavf/lavc components*/
    av_register_all();
    frame = av_frame_alloc();
    frame2 = av_frame_alloc();

    /* slurp file content into buffer */
    ret = av_file_map( input_filename, &buffer, &buffer_size, 0, NULL );
    if (ret < 0)
        return 1;

    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = buffer;
    bd.size = buffer_size;

    if ( !( fmt_ctx = avformat_alloc_context() ) )
    {
        ret = AVERROR( ENOMEM );
        return 1;
    }

    avio_ctx_buffer = ( uint8_t* )av_malloc( avio_ctx_buffer_size );
    if ( !avio_ctx_buffer )
    {
        ret = AVERROR( ENOMEM );
        return 1;
    }
    avio_ctx = avio_alloc_context( avio_ctx_buffer, avio_ctx_buffer_size, 0, &bd, &read_packet, NULL, NULL );
    if ( !avio_ctx )
    {
        ret = AVERROR( ENOMEM );
        return 1;
    }
    fmt_ctx->pb = avio_ctx;

    ret = avformat_open_input( &fmt_ctx, NULL, NULL, NULL );
    if ( ret < 0 )
    {
        fprintf(stderr, "Could not open input\n");
        return 1;
    }

    ret = avformat_find_stream_info( fmt_ctx, NULL );
    if ( ret < 0 )
    {
        fprintf( stderr, "Could not find stream information\n" );
        return 1;
    }

    ret = av_find_best_stream( fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0 );
    if( ret < 0 )
    {
        fprintf( stderr, "Cannot find a video stream in: '%s'\n", input_filename );
        return 1;
    }

    fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_DISCARD_CORRUPT | AVFMT_FLAG_FLUSH_PACKETS;

    video_stream_index = ret;
    dec_ctx = fmt_ctx->streams[video_stream_index]->codec;
    dec_ctx->thread_count = sysconf( _SC_NPROCESSORS_ONLN );
    /* init the video decoder */
    if( ( ret = avcodec_open2( dec_ctx, dec, NULL ) ) < 0 )
    {
        fprintf( stderr, "idCinematic: Cannot open video decoder for: '%s'\n", input_filename );
        return 1;
    }

    av_dump_format( fmt_ctx, 0, input_filename, 0 );

    img_width  = dec_ctx->width;
    img_height = dec_ctx->height;

    image = ( BYTE* )malloc( img_width * img_height * 4 * 2 );

    avpicture_fill( ( AVPicture* )frame2, image, PIX_FMT_BGR32, img_width, img_height );

    img_convert_ctx = sws_getContext( dec_ctx->width, dec_ctx->height, AV_PIX_FMT_YUV444P, img_width, img_height, PIX_FMT_BGR32, SWS_BICUBIC, NULL, NULL, NULL );

    makeSurface();
    initGL();
    resizeGL( disp_width, disp_height );

    while( file_size )
    {
        decodeFrame();
        renderGL();
    }

    avformat_close_input( &fmt_ctx );
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if ( avio_ctx )
    {
        av_freep( &avio_ctx->buffer );
        av_freep( &avio_ctx );
    }
    av_file_unmap( buffer, buffer_size );

    free( image );
    return 0;
}
