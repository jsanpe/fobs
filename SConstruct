import os
import sys
import subprocess
sys.path += [sys.path[0]+'/scripts',sys.path[0]+'/scons-local']
import fobs
import fobscons


ffmpeg_path=fobs.FFMPEG_HOME
full_ffmpeg_path= os.path.abspath(ffmpeg_path.replace('#',fobs.root+"/"))

env_ffmpeg = Environment(ENV=os.environ)
if env_ffmpeg['PLATFORM']=='win32':
	env_ffmpeg = Environment(tools=['mingw','javac','jar'])
	env_ffmpeg['ENV']['PATH'] = os.environ['PATH']

#print env_ffmpeg['TOOLS']

env_ffmpeg.Append(ENV={'PKG_CONFIG_PATH':os.path.abspath(full_ffmpeg_path+'/lib/pkgconfig')})
env_ffmpeg.Append(CPPDEFINES={'__STDC_CONSTANT_MACROS':'', 'attribute_deprecated':''})


# for i in ['CPPPATH', 'LIBPATH', 'LIBS']:
# 	print "Environment['",i,"']=",env_ffmpeg[i]

if not env_ffmpeg.GetOption('clean'):
	if env_ffmpeg['PLATFORM'] == 'win32':
		#Run python version of pkg-config
		res = fobscons.pkgconfig('libavformat',[env_ffmpeg['ENV']['PKG_CONFIG_PATH']])
		env_ffmpeg.MergeFlags(res.get_output())
		env_ffmpeg.Append(LIBS=['swscale'])
		for i in ['CPPPATH', 'LIBPATH']:
			env_ffmpeg[i]=[os.path.abspath(fobs.SYSTEM_ROOT+'/'+item) for item in env_ffmpeg[i]]		
	else:
		res = fobscons.CustomParseConfig(env_ffmpeg,'pkg-config --cflags --libs libavformat')
		env_ffmpeg.Append(LIBS=['pthread','swscale'])
	
	conf = Configure(env_ffmpeg)
	if not conf.CheckHeader('stdint.h','<>','cpp'):
		print 'FFMpeg headers could not be found'
		Exit(1)
	if not conf.CheckHeader('libavcodec/avcodec.h','""'):
		print 'FFMpeg headers could not be found'
		Exit(1)
	if not conf.CheckHeader('libavformat/avformat.h','""'):
		print 'FFMpeg headers could not be found'
		Exit(1)
	if not conf.CheckHeader('libavformat/avi.h','""'):
		print 'libavformat/avi.h was not found. Copy it from your ffmpeg sources package'
		Exit(1)
	if not conf.CheckHeader('libavformat/riff.h','""'):
		print 'libavformat/riff.h was not found. Copy it from your ffmpeg sources package'
		Exit(1)
	if not conf.CheckHeader('libswscale/swscale.h','""'):
		print 'libswscale/swscale.h was not found. Copy it from your ffmpeg sources package.'
		Exit(1)

	if not conf.CheckHeader('inttypes.h','""'):
		print 'Inttypes headers could not be found'
		Exit(1)
	if not conf.CheckType('int64_t','#include <stdint.h>'):
		print 'Inttypes not found'
		Exit(1)
	env_ffmpeg = conf.Finish()


if fobs.DEBUG_SYMBOLS == 'yes':
    env_ffmpeg.Append(LINKFLAGS=['-g'])
    env_ffmpeg.Append(CCFLAGS=['-g'])
if fobs.OPTIMIZATIONS == 'yes':
    env_ffmpeg.Append(LINKFLAGS=['-O3'])
    env_ffmpeg.Append(CCFLAGS=['-O3'])


env_ffmpeg = SConscript('core/SConstruct', exports='env_ffmpeg')
SConscript(['test/SConstruct'], exports='env_ffmpeg')

if fobs.FOBS4JMF=='yes':
	java_flag = fobscons.ConfigureJNI(env_ffmpeg)
	if java_flag:
		SConscript(['jmf/SConstruct'], exports='env_ffmpeg')
	else:
		print "\nFobs4jmf build was disabled. A valid JDK was not detected. Include \"/path/to/jdk/bin\" in your PATH."
