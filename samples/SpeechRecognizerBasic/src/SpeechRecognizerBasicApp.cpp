#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "sphinx/Recognizer.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class SpeechRecognizerBasicApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
	
	void speechEvent(const std::string& msg);
	
	float mMessageStart;
	std::string mMessage;
	sphinx::RecognizerRef mRecog;
};

void SpeechRecognizerBasicApp::setup()
{
	ci::fs::path hmmPath  = ci::app::getAssetPath( "en-us" );
	ci::fs::path dictPath = ci::app::getAssetPath( "cmudict-en-us.dict" );
	ci::fs::path lmPath   = ci::app::getAssetPath( "demo.jsgf" );
	
	mRecog = sphinx::Recognizer::create( hmmPath.string(), dictPath.string() );
	mRecog->connectEventHandler( std::bind( &SpeechRecognizerBasicApp::speechEvent, this, std::placeholders::_1 ) );
	mRecog->addModelJsgf( "primary", lmPath, true );
	mRecog->start();
}

void SpeechRecognizerBasicApp::mouseDown( MouseEvent event )
{
}

void SpeechRecognizerBasicApp::update()
{
	if( getElapsedSeconds() - mMessageStart >= 5.0f )
		mMessage.clear();
}

void SpeechRecognizerBasicApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	
	if( ! mMessage.empty() )
		gl::drawStringCentered( mMessage, getWindowCenter() );
}

void SpeechRecognizerBasicApp::speechEvent(const std::string& msg)
{
	mMessage = msg;
	mMessageStart = getElapsedSeconds();
}

CINDER_APP( SpeechRecognizerBasicApp, RendererGl )
