/******************************************************************************
* FOBS Java JMF Native PlugIn 
* This file contain the native implementation of the stand-alone codec that
* allows to use all the JMF internals along with ffmpeg. This file is included
* as of version 0.3 to provide support for custom DataSources. Some issues has
* been detected using this classes. See documentation for further details.
*
* Copyright (c) 2004 Omnividea Multimedia S.L
* Coded by Jos San Pedro Wandelmer
*
*    This file is part of FOBS.
*
*    FOBS is free software; you can redistribute it and/or modify
*    it under the terms of the GNU Lesser General Public License as
*    published by the Free Software Foundation; either version 2.1 
*    of the License, or (at your option) any later version.
*
*    FOBS is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with FOBS; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
******************************************************************************/

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#include "libavformat/avi.h"

#define __TAGS
#include "_tags.h"
#undef __TAGS
}

#include <jni.h>

#include "com_omnividea_media_codec_video_JavaDecoder.h"
#include "com_omnividea_media_codec_audio_JavaDecoder.h"
jboolean convertAudio
  (JNIEnv *env, jobject obj, jlong peer,
   jobject jinBuffer, jlong inBytes, jint inOffset,
   jobject joutBuffer, jlong outBytes, jlong size, jdouble dts);


static jlong ptr2jlong(void *ptr)
{
	jlong jl = 0;
	if (sizeof(void *) > sizeof(jlong))
	{	fprintf(stderr, "sizeof(void *) > sizeof(jlong)\n");
		return 0;
		//(* (int *) 0) = 0;	// crash.
	}
	
	memcpy(&jl, &ptr, sizeof(void *));
	return jl;
}

static void *jlong2ptr(jlong jl)
{
	
	void *ptr = 0;
	if (sizeof(void *) > sizeof(jlong))
	{	fprintf(stderr, "sizeof(void *) > sizeof(jlong)\n");
		return 0;
		//(* (int *) 0) = 0;	// crash.
	}
	
	memcpy(&ptr, &jl, sizeof(void *));
	return ptr;
}

/*
 * Wrapper structure that holds relevant data per codec instance.
 */
typedef struct {
	AVCodec *codec;
    //AVCodecParserContext *parser_context;
	AVCodecContext *codec_context;
	//AVFrame *picture;
    AVPicture picture;
    AVPicture rgbPicture;
    uint8_t *rgbBuf;

    uint8_t *frameBuf;
    int frameBufSize;
    
    int width;
    int height;

	/* temporary buffer used for encoding h263/rtp ONLY (i.e. not
	 * for encoding plain old h263). NULL during decode */
	void *encode_buf;
	int encode_buf_size;
    
    //Audio
    int lastAudioSize;
} FFMPEGWrapper;



CodecID getCodecId(char *t)
{
    CodecID id = CODEC_ID_NONE;
    int iTag = MKTAG(t[0], t[1], t[2], t[3]);

    id = codec_get_id(codec_bmp_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    
    id = codec_get_id(mov_video_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    
    id = codec_get_id(nsv_codec_video_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    
    
    id = codec_get_id(codec_wav_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    
    id = codec_get_id(mov_audio_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    
    id = codec_get_id(nsv_codec_audio_tags, iTag);
    if(id != CODEC_ID_NONE) return id;    


}

CodecID _getCodecId(char *tag)
{
    if(!strcasecmp(tag, "iv31") ||
        !strcasecmp(tag, "iv32"))
    {
        return CODEC_ID_INDEO3;
    }
    /*
    else if(!strcasecmp(tag, "msvc") ||
        !strcasecmp(tag, "cram") ||
        !strcasecmp(tag, "wham"))
    {
        return CODEC_ID_MSVIDEO1;
    }*/
    else if(!strcasecmp(tag, "wmv1"))
    {
        return CODEC_ID_WMV1;
    }
    else if(!strcasecmp(tag, "wmv2"))
    {
        return CODEC_ID_WMV2;
    }
    else if(!strcasecmp(tag, "mpeg") ||
        !strcasecmp(tag, "mpg1") ||
        !strcasecmp(tag, "pim1") ||
        !strcasecmp(tag, "vcr2") ||
        !strcasecmp(tag, "mpg2"))
    {
        return CODEC_ID_MPEG1VIDEO;
    }
    else if(!strcasecmp(tag, "mjpa") ||
        !strcasecmp(tag, "mjpb") ||
        !strcasecmp(tag, "mjpg") ||
        !strcasecmp(tag, "ljpg") ||
        !strcasecmp(tag, "avdj") ||
        !strcasecmp(tag, "jpgl"))
    {
        return CODEC_ID_MJPEG;
    }
    else if(!strcasecmp(tag, "svq1") ||
        !strcasecmp(tag, "svqi"))
    {
        return CODEC_ID_SVQ1;
    }
    else if(!strcasecmp(tag, "sv31"))
    {
        return CODEC_ID_SVQ3;
    }
    else if(!strcasecmp(tag, "mp4v") ||
        !strcasecmp(tag, "divx") ||
        !strcasecmp(tag, "dx50") ||
        !strcasecmp(tag, "xvid") ||
        !strcasecmp(tag, "mp4s") ||
        !strcasecmp(tag, "div1") ||
        !strcasecmp(tag, "blz0") ||
        !strcasecmp(tag, "ump4") ||
        !strcasecmp(tag, "m4s2"))
    {
        return CODEC_ID_MPEG4;
    }
    else if(!strcasecmp(tag, "h264"))
    {
        return CODEC_ID_H264;
    }        
    else if(!strcasecmp(tag, "h263"))
    {
        return CODEC_ID_H263;
    }        
    else if(!strcasecmp(tag, "u263") ||
        !strcasecmp(tag, "viv1"))
    {
        return CODEC_ID_H263P;
    }
/*    else if(!strcasecmp(tag, "i263"))
    {
        return CODEC_ID_I263;
    }  */
    else if(!strcasecmp(tag, "dvc") ||
        !strcasecmp(tag, "dvcp") ||
        !strcasecmp(tag, "dvsd") ||
        !strcasecmp(tag, "dvhs") ||
        !strcasecmp(tag, "dvs1") ||
        !strcasecmp(tag, "dv25"))
    {
        return CODEC_ID_DVVIDEO;
    }
    else if(!strcasecmp(tag, "vp31"))
    {
        return CODEC_ID_VP3;
    }  
    /*else if(!strcasecmp(tag, "rpza"))
    {
        return CODEC_ID_RPZA;
    }  
    else if(!strcasecmp(tag, "cvid"))
    {
        return CODEC_ID_CINEPAK;
    }  
    else if(!strcasecmp(tag, "smc"))
    {
        return CODEC_ID_SMC;
    }*/         
    else if(!strcasecmp(tag, "mp42")||
        !strcasecmp(tag, "div2"))
    {
        return CODEC_ID_MSMPEG4V2;
    }
    else if(!strcasecmp(tag, "mpg4"))
    {
        return CODEC_ID_MSMPEG4V1;
    }       
    else if(!strcasecmp(tag, "div3") ||
        !strcasecmp(tag, "mp43") ||
        !strcasecmp(tag, "mpg3") ||
        !strcasecmp(tag, "div5") ||
        !strcasecmp(tag, "div6") ||
        !strcasecmp(tag, "div4") ||
        !strcasecmp(tag, "ap41") ||
        !strcasecmp(tag, "col1") ||
        !strcasecmp(tag, "col0"))
    {
        return CODEC_ID_MSMPEG4V3;
    }
    return CODEC_ID_NONE;
}

bool isBigEndianSA()
{
    int tmp = 0x12345678;
    char *buf = (char*)(&tmp);
    //printf("%x %x %x %x\n", (int)buf[0], (int)buf[1], (int)buf[2], (int)buf[3]);
    if(buf[0]==0x78) return (jboolean)0;
    else return (jboolean)1;
}


/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    isBigEndian
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_isBigEndian
  (JNIEnv *env, jclass cl)
{
    return (jboolean)isBigEndianSA();
}

int FFMPEG_init = 0;

static void setOutputDone(JNIEnv *env, jobject obj, int done) {
    jclass clazz = env->GetObjectClass(obj);
    jfieldID fid = env->GetFieldID(clazz, "outputDone", "Z");
    env->SetIntField(obj, fid, (jboolean)done);
}


/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_init
  (JNIEnv *env, jobject obj)
{
    FFMPEGWrapper *wrapper;

    if(!FFMPEG_init) {
        avcodec_init();
        avcodec_register_all();
        FFMPEG_init = 1;
    }


    jclass clazz = env->GetObjectClass(obj);
    jfieldID fidPeer = env->GetFieldID(clazz, "peer", "J");   
    jlong peerVal = env->GetLongField(obj, fidPeer);
    if (peerVal != 0)
        return;
    

    wrapper = (FFMPEGWrapper *) av_malloc( sizeof(FFMPEGWrapper) );
    if(!wrapper)
        return;

    memset(wrapper,0,sizeof(FFMPEGWrapper));

    env->SetLongField(obj, fidPeer, ptr2jlong(wrapper));
#ifdef DEBUG
    av_log(NULL, AV_LOG_INFO, "obj initialized.\n");
#endif
    return;
}


/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    open_codec
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_open_1codec
  (JNIEnv *env, jobject obj, jlong peer, jstring codec_name)
{
    FFMPEGWrapper *wrapper;    
    enum CodecID codec_required;

    if (peer == 0)
        return (jboolean) 0;

    wrapper = (FFMPEGWrapper *) jlong2ptr(peer);

    // Find matching ffmpeg codec using the codec_name passed in
    const char *str = env->GetStringUTFChars(codec_name, 0);
#ifdef DEBUG
	av_log(NULL, AV_LOG_INFO, "open_codec called for %s\n", str);
#endif
    av_log(NULL, AV_LOG_INFO, "Encoding: %s\n", str);

    //codec_required = getCodecId((char*)str);
    codec_required = getCodecId((char*)str);
    env->ReleaseStringUTFChars(codec_name, str);
    if (codec_required == CODEC_ID_NONE)
    {
        av_log(NULL, AV_LOG_INFO, "Codec NOT Found!\n");
        return (jboolean) 0;
    }
    av_log(NULL, AV_LOG_INFO, "Codec Found: %d\n", codec_required);


    /* find the video decoder */
    wrapper->codec = avcodec_find_decoder(codec_required);
    if (!wrapper->codec ) {
        //fprintf(stderr, "codec not found\n");
        return (jboolean) 0;
    }

	// Allocate and zero the codec context and frame. Remember
	// them in the wrapper structure.
	AVCodecContext *context = avcodec_alloc_context();
	wrapper->codec_context  = context;
    
    //try to alloc some extradata space, just in case
    context->extradata = (uint8_t*) av_malloc(200); //solve some issues with SVQ3 codec (and maybe others)
    context->time_base.num = 1;
    context->time_base.num = 30000;
    context->sample_aspect_ratio.num = 1;
    context->sample_aspect_ratio.den = 1;
    //wrapper->codec_context->bit_rate = 0;
    //wrapper->codec_context->codec_tag = (str[3] << 24)|(str[2] << 16)|(str[1] << 8)|str[0];
    //wrapper->codec_context->frame_rate_base = 1001;
    //wrapper->codec_context->frame_rate *= 1001;
	//wrapper->picture        = avcodec_alloc_frame();
    //context->flags |= CODEC_FLAG_EMU_EDGE;
    //context->flags |= CODEC_FLAG_TRUNCATED;

    /* for some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because these info are not available
       in the bitstream */
    if(wrapper->codec_context->width ==0)
        wrapper->codec_context->width = wrapper->width;
    if(wrapper->codec_context->height == 0)
        wrapper->codec_context->height = wrapper->height;

    /* open it */
    wrapper->codec_context->codec_id = codec_required;
    if (avcodec_open(wrapper->codec_context, wrapper->codec) < 0) {
        //fprintf(stderr, "could not open codec\n");
        return (jboolean) 0;
    }    
    //wrapper->parser_context = av_parser_init(codec_required);
    
    av_log(NULL, AV_LOG_INFO, "Codec opened...W=%d H=%d\n", wrapper->codec_context->width, wrapper->codec_context->height);
#ifdef DEBUG
    av_log(NULL, AV_LOG_INFO, "open_codec successful!\n");
#endif

    return (jboolean) 1;
}




unsigned char* getRGB(AVPicture *decodedPicture, FFMPEGWrapper *wrapper, int *outBuf)//, int sizeOutBuf)
{
   int ret;
   int w = 0, h = 0;
   unsigned offset = 0;
   AVPicture *tmpPicture;
   enum PixelFormat pix_fmt=PIX_FMT_RGB24;
   AVCodecContext *dec = wrapper->codec_context;
   
   /* convert pixel format if needed */
   if(pix_fmt == dec->pix_fmt) 
   {
    av_log(NULL, AV_LOG_INFO, "Same PIXFMT!\n");
      tmpPicture = decodedPicture;
   }
   else
   {
      /* create temporary picture */
      if(wrapper->rgbBuf == NULL)
      {
         int size = avpicture_get_size(pix_fmt, dec->width, dec->height);
         wrapper->rgbBuf = (uint8_t *)av_malloc(size);
         if (!wrapper->rgbBuf)
         {
            return NULL;
         }
         avpicture_fill(&(wrapper->rgbPicture), wrapper->rgbBuf, pix_fmt, dec->width, dec->height);
      }
      /*
      int size = avpicture_get_size(pix_fmt, dec->width, dec->height);
      if(size > sizeOutBuf)
      {
        printf("FFMPEG PLUGIN: Not enough buffer(%d, %d)!!!!\n", size, sizeOutBuf);
        return NULL;
      }
      avpicture_fill(&(wrapper->rgbPicture), outBuf, pix_fmt, dec->width, dec->height);*/
      //ret = img_convert(&(wrapper->rgbPicture), pix_fmt, 
      //                 decodedPicture, dec->pix_fmt, 
      //                 dec->width, dec->height);
      if(ret < 0) 
      {
         return NULL;
      }
      
      tmpPicture = &(wrapper->rgbPicture);
   }
   return (unsigned char *)tmpPicture->data[0];
}



/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    init_decoding
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_init_1decoding
  (JNIEnv *env, jobject obj, jlong peer, jint width, jint height)
{
    FFMPEGWrapper *wrapper;    
    int got_picture, len, inBuf_size;

	if (peer == 0)
		return;
    av_log(NULL, AV_LOG_INFO, "Setting size - %dx%d\n", width, height);
    wrapper = (FFMPEGWrapper *) jlong2ptr(peer);
    wrapper->width = width;
    wrapper->height = height;
    
    //if(wrapper->frameBuf != NULL) free(wrapper->frameBuf);
    //wrapper->frameBuf = NULL;// (unsigned char*)malloc(width * height * 3);
}


/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    convert
 * Signature: (ILjava/lang/Object;JILjava/lang/Object;JJD)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_convert
  (JNIEnv *env, jobject obj, jlong peer,
   jobject jinBuffer, jlong inBytes, jint inOffset,
   jobject joutBuffer, jlong outBytes, jlong size, jdouble dts)
{
    static int counter = 0;
	unsigned char *inBuf  = (unsigned char *) inBytes;
	int *outBuf = (int *) outBytes;
    unsigned char *inBuf_ptr = NULL;
	int outputDone = 0; // false by default
    FFMPEGWrapper *wrapper;    
    int got_picture, len, inBuf_size;
    AVCodecContext *c;
//    int64_t ffDts = dts * 1000000;
    jlong outBufferSize = outBytes;

	if (peer == 0)
		return (jboolean) 0;

    wrapper = (FFMPEGWrapper *) jlong2ptr(peer);
    c = wrapper->codec_context;
    
    //if(c->codec_type == CODEC_TYPE_AUDIO) return convertAudio(env, obj, peer, jinBuffer, inBytes, inOffset, joutBuffer, outBytes, size, dts);

    //printf("OutputBuffer Size: %ld\n", (long)outBytes);
    if(c->width == 0 && c->height == 0)
    {
        //printf("Setting W%H in codec context:%dx%d\n",wrapper->width, wrapper->height);
        c->width = wrapper->width;
        c->height = wrapper-> height;
    }
    
    if (inBytes == 0)
        inBuf = (unsigned char *) env->GetByteArrayElements((jbyteArray) jinBuffer, NULL);

    if (dts > 0 && outBytes == 0)
    {
        outBuf = (int *) env->GetIntArrayElements((jintArray) joutBuffer, NULL);
        //outBufferSize = env->GetArrayLength((jbyteArray)joutBuffer);
    } else if(dts < 0) {
        outBuf = (int *) env->GetDirectBufferAddress(joutBuffer);
    }

    inBuf_ptr = inBuf;
	inBuf_size = size;

    //printf("DTS: %lf %ld\n", dts, ffDts);

    inBuf_ptr += inOffset;
    got_picture = 0;
    while (inBuf_size > 0) {
//        unsigned long pts, dts;
//        printf("Trace 0: %p %p %p %p\n", wrapper->parser_context, wrapper->parser_context->parser, wrapper->codec, wrapper->frameBuf);
        //len = av_parser_parse(wrapper->parser_context, wrapper->codec_context, &(wrapper->frameBuf), &(wrapper->frameBufSize), inBuf_ptr, inBuf_size, ffDts, ffDts*1000);
        //inBuf_size -= len;
        //inBuf_ptr += len;        
        //printf("avdecode: len=%d bSize=%d frameBufSize=%d\n", len, inBuf_size, wrapper->frameBufSize);
        //if(wrapper->frameBufSize <= 0) continue;
        AVFrame big_picture;
        //len = avcodec_decode_video(c, wrapper->picture, &got_picture,  wrapper->frameBuf, wrapper->frameBufSize);
        len = avcodec_decode_video(c, &big_picture, &got_picture,  inBuf_ptr, inBuf_size);
        /*if(len != inBuf_size)
        {
        }*/
        inBuf_size -= len;
        inBuf_ptr += len;  
        inBuf_size = 0; //FIX 0.4.1      
        //printf("avdecode: len=%d bSize=%d frameBufSize=%d\n", len, inBuf_size, wrapper->frameBufSize);
        
        /*if (len < 0) {
            //fprintf(stderr, "Error while decoding frame \n");
            return (jboolean) 0;
        }*/
        if (len >= 0 && got_picture) {
            wrapper->picture = *(AVPicture*)&big_picture;
            //printf("Hey...there's a frame!!");
            unsigned int rgbSize = wrapper->codec_context->width * wrapper->codec_context->height;
            
            
            //Code for wrapper->getRGBA((char*)outBuf);
            int ret;
            enum PixelFormat pix_fmt=PIX_FMT_RGBA;
            AVPicture tmpPicture;
            avpicture_fill(&tmpPicture, (uint8_t *)outBuf, pix_fmt, c->width, c->height);    
            //ret = img_convert(&tmpPicture, pix_fmt, &(wrapper->picture), c->pix_fmt,c->width, c->height);
            if(ret < 0) 
            {
                 av_log(NULL, AV_LOG_INFO, "Error Converting into RGBA\n");
            }


            
            
            /*
            unsigned char *buffer = getRGB(&(wrapper->picture), wrapper, outBuf);//, outBufferSize);
            if(buffer == NULL)
            {
                av_log(NULL, AV_LOG_INFO, "Error converting into RGB\n");
                return (jboolean)0;
            }
            //JpegLib::rgb2jpeg(buffer, wrapper->codec_context->width, wrapper->codec_context->height, (String("frame_")+String::intToStr(counter++)+String(".jpg")).c_str());
            unsigned int i = 0, offset = 0, offsetRGB = 0, w = 0, h = 0;
            offsetRGB = 0;
            *outBuf = 0;

            int* dst = outBuf;//(int*)(((char *)outBuf)+1);
            unsigned char* src = (unsigned char*)buffer;
            bool flag = isBigEndianSA();
            if(flag)
            {
                for(; i < rgbSize; i++)
                {
                    memcpy(((char*)dst)+1, src, 3);
                    dst++;
                    src+=3;
                    
                    //*dst = (*src)<<16 | (*(++src))<<8 | (*(++src));
                    //src++;

                }
            }
            else
            {
                for(; i < rgbSize; i++)
                {
                    
                    //memcpy(((char*)dst)+1, src, 3);
                    //dst++;
                    //src+=3;
                    
                    *dst = (*src)<<16 | (*(src+1))<<8 | (*(src+2));
                    src+=3;
                    dst++;

                }            
            }*/

            outputDone = 1;
        }
    }
	// export the outputDone variable to tell JMF when the output buffer has
	// been filled with data.
	setOutputDone(env, obj, outputDone);

    if (outBytes == 0)
        if(dts > 0)env->ReleaseIntArrayElements((jintArray) joutBuffer, (jint *) outBuf, 0);
    if (inBytes == 0)
        env->ReleaseByteArrayElements((jbyteArray) jinBuffer, (jbyte *) inBuf, JNI_ABORT);

    jboolean retCode = len >=0;
    return retCode; 
}

jboolean convertAudio
  (JNIEnv *env, jobject obj, jlong peer,
   jobject jinBuffer, jlong inBytes, jint inOffset,
   jobject joutBuffer, jlong outBytes, jlong size, jdouble dts)
{
    static int counter = 0;
	unsigned char *inBuf  = (unsigned char *) inBytes;
	int *outBuf = (int *) outBytes;
    unsigned char *inBuf_ptr = NULL;
	int outputDone = 0; // false by default
    FFMPEGWrapper *wrapper;    
    int got_picture, len, inBuf_size;
    AVCodecContext *c;
    int64_t ffDts = (int64_t) (dts * 1000000);
    jlong outBufferSize = outBytes;

	if (peer == 0)
		return (jboolean) 0;

    wrapper = (FFMPEGWrapper *) jlong2ptr(peer);
    c = wrapper->codec_context;
    
    if (inBytes == 0)
        inBuf = (unsigned char *) env->GetByteArrayElements((jbyteArray) jinBuffer, NULL);

    if (outBytes == 0)
    {
        outBuf = (int *) env->GetIntArrayElements((jintArray) joutBuffer, NULL);
        //outBufferSize = env->GetArrayLength((jbyteArray)joutBuffer);
    }

    inBuf_ptr = inBuf;
	inBuf_size = size;

    //printf("DTS: %lf %ld\n", dts, ffDts);

    inBuf_ptr += inOffset;
    got_picture = 0;
    while (inBuf_size > 0) {
        //short decodedSamples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        int data_size = 0;

        //len = avcodec_decode_audio(c, (int16_t*)outBuf, &data_size, inBuf_ptr, inBuf_size);
        wrapper->lastAudioSize = data_size;

        inBuf_size -= len;
        inBuf_ptr += len;        
        //printf("avdecode: len=%d bSize=%d frameBufSize=%d\n", len, inBuf_size, wrapper->frameBufSize);
        
        if (len < 0) {
            //fprintf(stderr, "Error while decoding frame \n");
            return (jboolean) 0;
        }
        if (data_size>0) {

            outputDone = 1;
        }
    }
	// export the outputDone variable to tell JMF when the output buffer has
	// been filled with data.
	setOutputDone(env, obj, outputDone);

    if (outBytes == 0)
        env->ReleaseIntArrayElements((jintArray) joutBuffer, (jint *) outBuf, 0);
    if (inBytes == 0)
        env->ReleaseByteArrayElements((jbyteArray) jinBuffer, (jbyte *) inBuf, JNI_ABORT);

    return (jboolean) 1; 
}

/*
 * Class:     com_omnividea_media_codec_video_JavaDecoder
 * Method:    close_codec
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_video_JavaDecoder_close_1codec
  (JNIEnv *env, jobject obj, jlong peer)
{
	FFMPEGWrapper *wrapper;    

	if (peer == 0)
		return (jboolean) 0;

	wrapper = (FFMPEGWrapper *) jlong2ptr(peer);

    //av_parser_close(wrapper->parser_context);
    if(wrapper->codec_context)
    {
        av_free(wrapper->codec_context->extradata); //solve some issues with SVQ3 codec (and maybe others)
        avcodec_close(wrapper->codec_context);
        av_free(wrapper->codec_context);
    }
    /*
    if(wrapper->parser_context)
    {
        free(wrapper->parser_context);
    }*/

	//free(wrapper->picture);
    if(wrapper->rgbBuf) av_free(wrapper->rgbBuf);
    //if(wrapper->frameBuf)free(wrapper->frameBuf);
	if(wrapper) av_free(wrapper);
    wrapper = NULL;

	// Make sure the "peer" variable is zeroed in java class
    jclass clazz = env->GetObjectClass(obj);
    jfieldID fidPeer = env->GetFieldID(clazz, "peer", "J");
    env->SetLongField(obj, fidPeer, ptr2jlong(0));

#ifdef DEBUG
	av_log(NULL, AV_LOG_INFO, "close_codec finished\n");
#endif
	return (jboolean) 1; 
}






/* Inaccessible static: bigEndian */
/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    isBigEndian
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_isBigEndian
  (JNIEnv *env , jclass cl)
{
    return Java_com_omnividea_media_codec_video_JavaDecoder_isBigEndian(env , cl);
}

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_init
  (JNIEnv *env, jobject obj)
{
    return Java_com_omnividea_media_codec_video_JavaDecoder_init(env, obj);
}

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    open_codec
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_open_1codec
  (JNIEnv *env, jobject obj, jlong peer, jstring codec_name)
{
    return Java_com_omnividea_media_codec_video_JavaDecoder_open_1codec(env, obj, peer, codec_name);
}

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    close_codec
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_close_1codec
  (JNIEnv *env, jobject obj, jlong peer)
{
    return Java_com_omnividea_media_codec_video_JavaDecoder_close_1codec(env, obj, peer);
}

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    init_decoding
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_init_1decoding
  (JNIEnv *env, jobject obj, jlong peer, jint width, jint height)
  {
    return Java_com_omnividea_media_codec_video_JavaDecoder_init_1decoding(env, obj, peer, width,  height);
  }

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    convert
 * Signature: (ILjava/lang/Object;JILjava/lang/Object;JJD)Z
 */
JNIEXPORT jboolean JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_convert
  (JNIEnv *env, jobject obj, jlong peer,
   jobject jinBuffer, jlong inBytes, jint inOffset,
   jobject joutBuffer, jlong outBytes, jlong size, jdouble dts) 
{
    return convertAudio(env,  obj,  peer, jinBuffer,  inBytes,  inOffset,
             joutBuffer, outBytes, size, dts);

}

/*
 * Class:     com_omnividea_media_codec_audio_JavaDecoder
 * Method:    lastAudioSize
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_omnividea_media_codec_audio_JavaDecoder_lastAudioSize
  (JNIEnv *env, jobject obj, jlong peer)
{
    FFMPEGWrapper *wrapper;    
	if (peer == 0)
		return -1;
    wrapper = (FFMPEGWrapper *) jlong2ptr(peer);
    return wrapper->lastAudioSize;
}


