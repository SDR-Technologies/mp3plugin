# mp3plugin
A MP3 writer plugin for the SDRVM. It receives IQ data and converts to mp3 stereo file, encoded at 48 kHz in this version

See example.js

## Compile

The following libs are required :
* mp3lame (libmp3lame-dev)
* samplerate (libsamplerate0-dev)

* clone
* mkdir build
* cd build
* cmake ../
* make 
* sudo make install

Installation will write to the default location of the SDRVM install : /opt/vmbase/extensions
Update CMakeLists if you want another location.



