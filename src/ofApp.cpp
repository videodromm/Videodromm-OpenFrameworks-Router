#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetVerticalSync(true);
    // setup a server with default options on port 9092
    // - pass in true after port to set up with SSL
    //bSetup = server.setup( 9093 );
#ifdef _WIN32 || _WIN64
    HWND m_hWnd = WindowFromDC(wglGetCurrentDC());
    LONG style = ::GetWindowLong(m_hWnd, GWL_STYLE);
    style &= ~WS_DLGFRAME;
    style &= ~WS_CAPTION;
    style &= ~WS_BORDER;
    
    LONG exstyle = ::GetWindowLong(m_hWnd, GWL_EXSTYLE);
    exstyle &= ~WS_EX_DLGMODALFRAME;
    
    ::SetWindowLong(m_hWnd, GWL_STYLE, style);
    ::SetWindowLong(m_hWnd, GWL_EXSTYLE, exstyle);
    
    SetWindowPos(m_hWnd,
                 HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE);
#endif
    ofxLibwebsockets::ServerOptions options = ofxLibwebsockets::defaultServerOptions();
    options.port = 9092;
    options.bUseSSL = false; // you'll have to manually accept this self-signed cert if 'true'!
    bSetup = server.setup(options);
    
    // this adds your app as a listener for the server
    server.addListener(this);
    
    ofBackground(0);
    ofSetFrameRate(60);
    ofSetWindowPosition(10, 100);
    ofSetWindowPosition(2600, 0);
    ofSetFullscreen(true);
    // midi
    // print input ports to console
    midiIn.listPorts(); // via instance
    //ofxMidiIn::listPorts(); // via static as well
    if (midiIn.getNumPorts()) {
        for (int i = 0; i < midiIn.getNumPorts(); i++) {
            string pname = midiIn.getPortName(i);
            if (ofStringTimesInString(pname, "Launchpad")) {
                printf("Launchpad on %d\n", i);
                midiInLaunchpad.openPort(i);
                midiInLaunchpad.addListener(this);
            }
            if (ofStringTimesInString(pname, "nano")) {
                printf("nanoKontrol on %d\n", i);
                midiInNanoKontrol.openPort(i);
                midiInNanoKontrol.addListener(this);
            }
        }
    }
    // open port by number (you may need to change this)
    if (midiIn.openPort(3))
    {
        //midiIn
        //midiIn.openVirtualPort("ofxMidiIn Input"); // open a virtual port
        
        // don't ignore sysex, timing, & active sense messages,
        // these are ignored by default
        midiIn.ignoreTypes(false, false, false);
        
        // add ofApp as a listener
        midiIn.addListener(this);
        
        // print received messages to the console
        midiIn.setVerbose(true);
    }
    
    // openCv
#ifdef _USE_LIVE_VIDEO
    vidGrabber.setVerbose(true);
    vidGrabber.initGrabber(320, 240);
#else
    vidPlayer.loadMovie("fingers.mov");
    vidPlayer.play();
#endif
    
    colorImg.allocate(320, 240);
    grayImage.allocate(320, 240);
    grayBg.allocate(320, 240);
    grayDiff.allocate(320, 240);
    
    bLearnBackground = true;
    threshold = 80;
}

//--------------------------------------------------------------
void ofApp::update(){
    bool bNewFrame = false;
    
#ifdef _USE_LIVE_VIDEO
    vidGrabber.update();
    bNewFrame = vidGrabber.isFrameNew();
#else
    vidPlayer.update();
    bNewFrame = vidPlayer.isFrameNew();
#endif
    
    if (bNewFrame){
        
#ifdef _USE_LIVE_VIDEO
        colorImg.setFromPixels(vidGrabber.getPixels(), 320, 240);
#else
        colorImg.setFromPixels(vidPlayer.getPixels(), 320, 240);
#endif
        
        grayImage = colorImg;
        if (bLearnBackground == true){
            grayBg = grayImage;		// the = sign copys the pixels from grayImage into grayBg (operator overloading)
            bLearnBackground = false;
        }
        
        // take the abs value of the difference between background and incoming and then threshold:
        grayDiff.absDiff(grayBg, grayImage);
        grayDiff.threshold(threshold);
        
        // find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
        // also, find holes is set to true so we will get interior contours as well....
        contourFinder.findContours(grayDiff, 20, (340 * 240) / 3, 10, true);	// find holes
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    if (bSetup){
        ofDrawBitmapString("WebSocket server setup at " + ofToString(server.getPort()) + (server.usingSSL() ? " with SSL" : " without SSL"), 20, 260);
        
        ofSetColor(150);
        ofDrawBitmapString("Click anywhere to open up client example", 20, 280);
    }
    else {
        ofDrawBitmapString("WebSocket setup failed :(", 20, 260);
    }
    
    int x = 20;
    int y = 640;
    
    ofSetColor(0, 150, 0);
    ofDrawBitmapString("Console", x, 620);
    
    ofSetColor(255);
    for (int i = messages.size() - 1; i >= 0; i--){
        ofDrawBitmapString(messages[i], x, y);
        y += 20;
    }
    if (messages.size() > NUM_MESSAGES) messages.erase(messages.begin());
    
    ofSetColor(150, 0, 0);
    ofDrawBitmapString("Type a message, hit [RETURN] to send:", x, ofGetHeight() - 60);
    ofSetColor(255);
    ofDrawBitmapString(toSend, x, ofGetHeight() - 40);
    // midi
    int name = 0;
    int value = 0;
    switch (midiMessage.status)
    {
        case MIDI_CONTROL_CHANGE:
            name = midiMessage.control;
            value = midiMessage.value;
            break;
        case MIDI_NOTE_ON:
            name = midiMessage.pitch;
            value = midiMessage.velocity;
            break;
        case MIDI_NOTE_OFF:
            name = midiMessage.pitch;
            value = midiMessage.velocity;
            break;
    }
    
    text.str(""); // clear
    // lmap<float>(value, 0.0, 127.0, 0.0, 1.0) (from Cinder)
    float normalizedValue = value / 127;
    text << "{\"params\" :[{\"name\" : " << name << ",\"value\" : " << normalizedValue << "}]}";
    if (name != 0) server.send(text.str());
    text.str(""); // clear
    // draw the last recieved message contents to the screen
    text << "Received: " << ofxMidiMessage::getStatusString(midiMessage.status);
    ofDrawBitmapString(text.str(), 820, 20);
    text.str(""); // clear
    
    text << "channel: " << midiMessage.channel;
    ofDrawBitmapString(text.str(), 820, 34);
    text.str(""); // clear
    
    text << "pitch: " << midiMessage.pitch;
    ofDrawBitmapString(text.str(), 820, 48);
    text.str(""); // clear
    ofRect(820, 58, midiMessage.pitch, 20);
    
    text << "velocity: " << midiMessage.velocity;
    ofDrawBitmapString(text.str(), 820, 96);
    text.str(""); // clear
    ofRect(820, 105, midiMessage.velocity, 20);
    
    text << "control: " << midiMessage.control;
    ofDrawBitmapString(text.str(), 820, 144);
    text.str(""); // clear
    ofRect(820, 154, midiMessage.control, 20);
    
    text << "value: " << midiMessage.value;
    ofDrawBitmapString(text.str(), 820, 192);
    text.str(""); // clear
    ofRect(820, 202, midiMessage.value, 20);
    
    
    text << "delta: " << midiMessage.deltatime;
    ofDrawBitmapString(text.str(), 820, 240);
    text.str(""); // clear
    
    //openCv
    
    // draw the incoming, the grayscale, the bg and the thresholded difference
    ofSetHexColor(0xffffff);
    colorImg.draw(20, 20);
    grayImage.draw(360, 20);
    grayBg.draw(20, 280);
    grayDiff.draw(360, 280);
    
    // then draw the contours:
    
    ofFill();
    ofSetHexColor(0x333333);
    ofRect(360, 540, 320, 240);
    ofSetHexColor(0xffffff);
    
    // we could draw the whole contour finder
    //contourFinder.draw(360,540);
    
    // or, instead we can draw each blob individually from the blobs vector,
    // this is how to get access to them:
    for (int i = 0; i < contourFinder.nBlobs; i++){
        contourFinder.blobs[i].draw(360, 540);
        
        // draw over the centroid if the blob is a hole
        ofSetColor(255);
        if (contourFinder.blobs[i].hole){
            ofDrawBitmapString("hole",
                               contourFinder.blobs[i].boundingRect.getCenter().x + 360,
                               contourFinder.blobs[i].boundingRect.getCenter().y + 540);
        }
    }
    
    // finally, a report:
    ofSetHexColor(0xffffff);
    stringstream reportStr;
    reportStr << "bg subtraction and blob detection" << endl
    << "press ' ' to capture bg" << endl
    << "threshold " << threshold << " (press: +/-)" << endl
    << "num blobs found " << contourFinder.nBlobs << ", fps: " << ofGetFrameRate();
    ofDrawBitmapString(reportStr.str(), 20, 600);
}
//--------------------------------------------------------------
void ofApp::onConnect(ofxLibwebsockets::Event& args){
    cout << "on connected" << endl;
}

//--------------------------------------------------------------
void ofApp::onOpen(ofxLibwebsockets::Event& args){
    cout << "new connection open" << endl;
    messages.push_back("New connection from " + args.conn.getClientIP() + ", " + args.conn.getClientName());
}

//--------------------------------------------------------------
void ofApp::onClose(ofxLibwebsockets::Event& args){
    cout << "on close" << endl;
    messages.push_back("Connection closed");
}

//--------------------------------------------------------------
void ofApp::onIdle(ofxLibwebsockets::Event& args){
    cout << "on idle" << endl;
}

//--------------------------------------------------------------
void ofApp::onMessage(ofxLibwebsockets::Event& args){
    cout << "got message " << args.message << endl;
    
    // trace out string messages or JSON messages!
    if (!args.json.isNull()){
        messages.push_back("New message: " + args.json.toStyledString() + " from " + args.conn.getClientName());
    }
    else {
        messages.push_back("New message: " + args.message + " from " + args.conn.getClientName());
    }
    
    // echo server = send message right back!
    args.conn.send(args.message);
}

//--------------------------------------------------------------
void ofApp::onBroadcast(ofxLibwebsockets::Event& args){
    cout << "got broadcast " << args.message << endl;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    switch (key){
        case ' ':
            bLearnBackground = true;
            toSend += key;
            break;
        case '+':
            threshold++;
            if (threshold > 255) threshold = 255;
            toSend += key;
            break;
        case '-':
            threshold--;
            if (threshold < 0) threshold = 0;
            toSend += key;
            break;
        case OF_KEY_ESC:
            // quit
            exit();
            break;
        case OF_KEY_RETURN:
            // send to all clients
            server.send(toSend);
            messages.push_back("Sent: '" + toSend + "' to " + ofToString(server.getConnections().size()) + " websockets");
            toSend = "";
            break;
        case OF_KEY_BACKSPACE:
            if (toSend.length() > 0){
                toSend.erase(toSend.end() - 1);
            }
            break;
        default:
            toSend += key;
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    string url = "http";
    if (server.usingSSL()){
        url += "s";
    }
    url += "://localhost:" + ofToString(server.getPort());
    ofLaunchBrowser(url);
}
void ofApp::newMidiMessage(ofxMidiMessage& msg) {
    
    // make a copy of the latest message
    midiMessage = msg;
}
void ofApp::exit() {
    
    // clean up
    midiIn.closePort();
    midiIn.removeListener(this);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
