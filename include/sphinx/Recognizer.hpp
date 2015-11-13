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

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>

#include <iostream>

#include <pocketsphinx.h>

#include <sphinxbase/jsgf.h>
#include <sphinxbase/fsg_model.h>

#include "cinder/Filesystem.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"
#include "cinder/audio/dsp/Converter.h"

namespace sphinx {
	
	typedef std::shared_ptr<class Recognizer>	RecognizerRef;
	typedef std::shared_ptr<class EventHandler>	EventHandlerRef;
	typedef std::shared_ptr<class Model>		ModelRef;
	
	/** @brief event handler abstract base class */
	class EventHandler
	{
	  public:
		
		/** @brief default constructor */
		EventHandler() { /* no-op */ }
		
		/** @brief virtual destructor */
		virtual ~EventHandler() { /* no-op */ }
		
		/** @brief pure virtual event function */
		virtual void event(ps_decoder_t* decoder) = 0;
	};
	
	/** @brief basic event handler */
	class EventHandlerBasic : public EventHandler
	{
	  public:
		
		typedef std::function<void(const std::string&)> CallbackFn;
	
	  private:
		
		CallbackFn mCb;
		
	  public:
		
		/** @brief default constructor */
		EventHandlerBasic(const CallbackFn& fn) : mCb( fn ) { /* no-op */ }
		
		/** @brief pure virtual event function */
		void event(ps_decoder_t* decoder);
	};
	
	/** @brief word segmentation event handler */
	class EventHandlerSegment : public EventHandler
	{
	  public:
		
		typedef std::function<void(const std::vector<std::string>&)> CallbackFn;
		
	  private:
		
		CallbackFn mCb;
		
	  public:
		
		/** @brief default constructor */
		EventHandlerSegment(const CallbackFn& fn) : mCb( fn ) { /* no-op */ }
		
		/** @brief pure virtual event function */
		void event(ps_decoder_t* decoder);
	};
	
	/** @brief word segmentation confidence event handler */
	class EventHandlerSegmentConfidence : public EventHandler
	{
	  public:
		
		typedef std::function<void(const std::vector<std::pair<std::string,float> >&)> CallbackFn;
		
	  private:
		
		CallbackFn mCb;
		
	  public:
		
		/** @brief default constructor */
		EventHandlerSegmentConfidence(const CallbackFn& fn) : mCb( fn ) { /* no-op */ }
		
		/** @brief pure virtual event function */
		void event(ps_decoder_t* decoder);
	};
	
	/** @brief language model base class */
	class Model
	{
  	  public:
		
		/** @brief default constructor */
		Model() { /* no-op */ }
		
		/** @brief virtual destructor */
		virtual ~Model() { /* no-op */ }
	};
	
	/** @brief FSG language model */
	class ModelFsg : public Model
	{
	  private:
	
		fsg_model_t* mModel;
		
	  public:
		
		/** @brief constructor */
		ModelFsg(fsg_model_t* model) : mModel( model ) { /* no-op */ }
		
		/** @brief destructor */
		~ModelFsg() { fsg_model_free( mModel ); }
	};
	
	/** @brief speech recognizer */
	class Recognizer
	{
	  private:
		
		EventHandlerRef						mHandler;		//!< event handler
		std::atomic<bool>					mStop;			//!< runner flag
		std::thread							mThread;		//!< runner thread
		
		cmd_ln_t*							mConfig;		//!< pocketsphinx config
		ps_decoder_t*						mDecoder;		//!< pocketsphinx decoder
		std::map<std::string,ModelRef>		mModelMap;		//!< language model map
		
		ci::audio::InputDeviceNodeRef		mInputNode;		//!< audio input node
		ci::audio::MonitorNodeRef			mMonitorNode;	//!< audio monitor node
						
		Recognizer(Recognizer const&) = delete;
		Recognizer& operator=(Recognizer const&) = delete;
		
		/** @brief private default constructor */
		Recognizer();
		
		/** @brief private initialization method */
		void initialize(const ci::fs::path& hmmPath, const ci::fs::path& dictPath);
		
		/** @brief private runner method */
		void run();
		
	  public:
		
		/** @brief static creational method */
		static RecognizerRef create(const ci::fs::path& hmmPath, const ci::fs::path& dictPath)
		{
			RecognizerRef r = RecognizerRef( new Recognizer() );
			r->initialize( hmmPath, dictPath );
			return r;
		}

		/** @brief destructor */
		~Recognizer();
		
		/** @brief connects generic event handler */
		void connectEventHandler(const EventHandlerRef& eventHandler);
		
		/** @brief connects basic event handler to recognizer */
		void connectEventHandler(const std::function<void(const std::string&)>& eventCb);
		
		/** @brief connects word segmentation event handler to recognizer */
		void connectEventHandler(const std::function<void(const std::vector<std::string>&)>& eventCb);
		
		/** @brief connects word segmentation confidence event handler to recognizer */
		void connectEventHandler(const std::function<void(const std::vector<std::pair<std::string,float> >&)>& eventCb);
		
		/** @brief adds model from JSGF filepath and associates it with key, optionally sets model active */
		void addModelJsgf(const std::string& key, const ci::fs::path& jsgfPath, bool setActive = true);
		
		/** @brief adds model from JSGF string and associates it with key, optionally sets model active */
		void addModelJsgf(const std::string& key, const std::string& jsgfData, bool setActive = true);
		
		/** @brief sets active model from key, throws if key is unfound */
		void setActiveModel(const std::string& key);
		
		/** @brief starts recognizer */
		void start();
	};
	
} // namespace sphinx
