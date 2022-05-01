var ok = loadPluginLib('/opt/vmbase/extensions/', 'libmp3filewriter');
if( ok == false ) {
    print('cannot load lib, stop.');
    exit();
}


var ok = ImportObject('libmp3filewriter', 'MP3Writer');
if( ok == false ) {
    print('Cannot load object');
    exit();
}


var encoder = new MP3Writer();
encoder.openfile('test.mp3');
encoder.write(DSP.tone( 440, 10000, 12500 ));
encoder.write(DSP.tone( 470, 10000, 12500 ));
encoder.write(DSP.tone( 490, 10000, 12500 ));
encoder.closefile();
