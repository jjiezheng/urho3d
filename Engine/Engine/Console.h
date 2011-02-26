//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef ENGINE_CONSOLE_H
#define ENGINE_CONSOLE_H

#include "EventListener.h"
#include "SharedPtr.h"

class BorderImage;
class Engine;
class Font;
class LineEdit;
class Text;
class XMLFile;

//! A console window with log history and AngelScript prompt
class Console : public RefCounted, public EventListener
{
public:
    //! Construct with an Engine pointer
    Console(Engine* engine);
    //! Destruct
    ~Console();
    
    //! Set UI elements' style from an XML file
    void setStyle(XMLFile* style);
    //! Show or hide. Showing automatically focuses the line edit
    void setVisible(bool enable);
    //! Toggle visibility
    void toggle();
    //! Set number of rows
    void setNumRows(unsigned rows);
    //! Update elements to layout properly. Call this after manually adjusting the sub-elements
    void updateElements();
    
    //! Return the UI style file
    XMLFile* getStyle() const { return mStyle; }
    //! Return the background element
    BorderImage* getBackground() const { return mBackground; }
    //! Return the line edit element
    LineEdit* getLineEdit() const { return mLineEdit; }
    //! Return whether is visible
    bool isVisible() const;
    //! Return number of rows
    unsigned getNumRows() const { return mRows.size(); }
    
private:
    //! Handle enter pressed on the line edit
    void handleTextFinished(StringHash eventType, VariantMap& eventData);
    //! Handle rendering window resize
    void handleWindowResized(StringHash eventType, VariantMap& eventData);
    //! Handle a log message
    void handleLogMessage(StringHash eventType, VariantMap& eventData);
    
    //! Engine
    Engine* mEngine;
    //! UI style file
    SharedPtr<XMLFile> mStyle;
    //! Background
    SharedPtr<BorderImage> mBackground;
    //! Text rows
    std::vector<SharedPtr<Text> > mRows;
    //! Line edit
    SharedPtr<LineEdit> mLineEdit;
};

#endif // ENGINE_CONSOLE_H
