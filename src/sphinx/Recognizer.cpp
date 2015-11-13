/*
 Copyright (c) 2015, Patrick J. Hebron
 All rights reserved.
 
 http://patrickhebron.com
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#include "sphinx/Recognizer.hpp"

namespace sphinx {
	
	static void loadTextFile(const ci::fs::path& filePath, std::string* output)
	{
		std::string line;
		std::ifstream fh( filePath.c_str() );
		if( fh.is_open() ) {
			while( std::getline( fh, line ) )
				*output += line + "\n";
			fh.close();
		}
		else {
			throw std::runtime_error( "Could not load file: \"" + filePath.string() + "\"" );
		}
	}
	
	static void convertFloatToInt16(const float* sourceArray, int16_t* destArray, size_t length)
	{
		const float intNormalizer = 32768.0f;
		
		for(size_t i = 0; i < length; i++) {
			destArray[ i ] = int16_t( sourceArray[i] * intNormalizer );
		}
	}
	
	void EventHandlerBasic::event(ps_decoder_t* decoder)
	{
		char const* message = ps_get_hyp( decoder, NULL );
		
		if( mCb != nullptr && message != NULL && strlen( message ) > 0 )
			mCb( std::string( message ) );
	}
	
	void EventHandlerSegment::event(ps_decoder_t* decoder)
	{
		std::vector<std::string> segments;
		
		ps_seg_t* iter = ps_seg_iter( decoder, NULL );
		
		while( iter != NULL ) {
			segments.push_back( std::string( ps_seg_word( iter ) ) );
			iter = ps_seg_next( iter );
		}
		
		if( ! segments.empty() )
			mCb( segments );
	}
	
	void EventHandlerSegmentConfidence::event(ps_decoder_t *decoder)
	{
		std::vector<std::pair<std::string,float> > segments;
		
		ps_seg_t* iter = ps_seg_iter( decoder, NULL );
		
		while( iter != NULL ) {
			int32 prob = ps_seg_prob( iter, NULL, NULL, NULL );
			segments.push_back( { std::string( ps_seg_word( iter ) ), logmath_exp( ps_get_logmath( decoder ), prob ) } );
			iter = ps_seg_next( iter );
		}
		
		if( ! segments.empty() )
			mCb( segments );
	}
	
	Recognizer::Recognizer() :
		mStop( false ),
		mThread(),
		mConfig( NULL ),
		mDecoder( NULL )
	{
		/* no-op */
	}
	
	void Recognizer::initialize(const ci::fs::path& hmmPath, const ci::fs::path& dictPath)
	{
		// Configure recognizer:
		mConfig = cmd_ln_init( NULL, ps_args(), true, "-hmm", hmmPath.c_str(), "-dict", dictPath.c_str(), "-logfn", "/dev/null", NULL );
		
		if( mConfig == NULL )
			throw std::runtime_error( "Could not configure speech recognizer" );
		
		// Initialize recognizer:
		mDecoder = ps_init( mConfig );
		
		if( mDecoder == NULL )
			throw std::runtime_error( "Could not initialize speech recognizer" );
	}
	
	void Recognizer::run()
	{
		// Create audio converter:
		auto converter = ci::audio::dsp::Converter::create( mMonitorNode->getSampleRate(), 16000, mMonitorNode->getNumChannels(), 1, mMonitorNode->getFramesPerBlock() );
		// Create buffer for converted audio:
		ci::audio::Buffer destBuffer( converter->getDestMaxFramesPerBlock(), converter->getDestNumChannels() );
		
		bool utt_started, in_speech;

		if( ps_start_utt( mDecoder ) < 0 )
			throw std::runtime_error( "Could not start utterance" );
		
		utt_started = false;
		
		while( ! mStop ) {
			// Convert buffer:
			std::pair<size_t,size_t> convertResult = converter->convert( &( mMonitorNode->getBuffer() ), &destBuffer );

			// Convert buffer data:
			int16_t* data = new int16_t[ convertResult.second ];
			convertFloatToInt16( destBuffer.getData(), data, convertResult.second );
			
			// Process buffer:
			ps_process_raw( mDecoder, data, convertResult.second, false, false );
			
			// Cleanup buffer data:
			delete[] data;
			
			in_speech = static_cast<bool>( ps_get_in_speech( mDecoder ) );
			
			if( in_speech && ! utt_started ) {
				utt_started = true;
			}
			
			if( ! in_speech && utt_started ) {
				// Start new utterance on speech to silence transition:
				ps_end_utt( mDecoder );
				
				// Pass to handler:
				if( mHandler )
					mHandler->event( mDecoder );
				
				// Prepare for next utterance:
				if( ps_start_utt( mDecoder ) < 0 )
					throw std::runtime_error( "Could not start utterance" );
				
				utt_started = false;
			}
			
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
		}
	}
	
	Recognizer::~Recognizer()
	{
		// Set stop flag:
		mStop = true;
		// Join thread:
		if( mThread.joinable() ) mThread.join();
		// Cleanup decoder:
		if( mDecoder ) ps_free( mDecoder );
		// Cleanup config:
		if( mConfig ) cmd_ln_free_r( mConfig );
		// Cleanup models:
		mModelMap.clear();
	}
	
	void Recognizer::connectEventHandler(const EventHandlerRef& eventHandler)
	{
		mHandler = eventHandler;
	}
	
	void Recognizer::connectEventHandler(const std::function<void(const std::string&)>& eventCb)
	{
		connectEventHandler( EventHandlerRef( new EventHandlerBasic( eventCb ) ) );
	}
	
	void Recognizer::connectEventHandler(const std::function<void(const std::vector<std::string>&)>& eventCb)
	{
		connectEventHandler( EventHandlerRef( new EventHandlerSegment( eventCb ) ) );
	}
	
	void Recognizer::connectEventHandler(const std::function<void(const std::vector<std::pair<std::string,float> >&)>& eventCb)
	{
		connectEventHandler( EventHandlerRef( new EventHandlerSegmentConfidence( eventCb ) ) );
	}
	
	void Recognizer::addModelJsgf(const std::string& key, const ci::fs::path& jsgfPath, bool setActive)
	{
		std::string data;
		loadTextFile( jsgfPath, &data );
		addModelJsgf( key, data, setActive );
	}
	
	void Recognizer::addModelJsgf(const std::string& key, const std::string& jsgfData, bool setActive)
	{
		// Create model:
		fsg_model_t* model = jsgf_read_string( jsgfData.c_str(), ps_get_logmath( mDecoder ), 7.5 );
		// Verify model creation:
		if( model == NULL )
			throw std::runtime_error( "Could not parse JSGF model" );
		// Add entry:
		mModelMap[ key ] = ModelRef( new ModelFsg( model ) );
		// Add model to decoder:
		ps_set_fsg( mDecoder, key.c_str(), model );
		// Set active, if flagged:
		if( setActive )
			ps_set_search( mDecoder, key.c_str() );
	}
	
	void Recognizer::setActiveModel(const std::string& key)
	{
		// Look for existing entry:
		auto findModel = mModelMap.find( key );
		// Remove existing entry, if applicable:
		if( findModel == mModelMap.end() )
			throw std::runtime_error( "Could not locate model \"" + key + "\"" );
		// Set model as cursor:
		ps_set_search( mDecoder, key.c_str() );
	}
	
	void Recognizer::start()
	{
		// Get audio context:
		auto ctx = ci::audio::Context::master();
		// Create input node:
		mInputNode = ctx->createInputDeviceNode();
		// Create monitor node:
		auto monitorFormat = ci::audio::MonitorNode::Format().windowSize( 1024 );
		mMonitorNode = ctx->makeNode( new ci::audio::MonitorNode( monitorFormat ) );
		// Attach monitor to input:
		mInputNode >> mMonitorNode;
		// Enable audio input device and context:
		mInputNode->enable();
		ctx->enable();
		// Start runner thread:
		mThread = std::thread( &Recognizer::run, this );
	}
	
} // namespace sphinx
