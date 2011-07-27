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

#pragma once

#include "Controls.h"
#include "HashSet.h"
#include "Object.h"
#include "ReplicationState.h"
#include "VectorBuffer.h"

#include <kNetFwd.h>
#include <kNet/SharedPtr.h>

#ifdef SendMessage
#undef SendMessage
#endif

class File;
class MemoryBuffer;
class Node;
class Scene;
class Serializable;

/// Queued remote event
struct RemoteEvent
{
    /// Receiver node ID (0 if not a remote node event)
    unsigned receiverID_;
    /// Event type
    StringHash eventType_;
    /// Event data
    VariantMap eventData_;
    /// In order -flag
    bool inOrder_;
};

/// Package file download
struct PackageDownload
{
    /// Construct with defaults
    PackageDownload();
    
    /// Destination file that is used to write the data
    SharedPtr<File> file_;
    /// Already received fragments
    HashSet<unsigned> receivedFragments_;
    /// Package name
    String name_;
    /// Total number of fragments
    unsigned totalFragments_;
    /// Checksum
    unsigned checksum_;
    /// Download initiated flag
    bool initiated_;
};

/// Connection in a networked scene
class Connection : public Object
{
    OBJECT(Connection);
    
public:
    /// Construct with context and kNet message connection pointers
    Connection(Context* context, bool isClient, kNet::SharedPtr<kNet::MessageConnection> connection);
    /// Destruct
    ~Connection();
    
    /// Send a message
    void SendMessage(int msgID, bool reliable, bool inOrder, const VectorBuffer& msg);
    /// Send a message
    void SendMessage(int msgID, bool reliable, bool inOrder, const unsigned char* data, unsigned numBytes);
    /// Send a message with content ID
    void SendMessage(int msgID, unsigned contentID, bool reliable, bool inOrder, const VectorBuffer& msg);
    /// Send a message with content ID
    void SendMessage(int msgID, unsigned contentID, bool reliable, bool inOrder, const unsigned char* data, unsigned numBytes);
    /// Send a remote event
    void SendRemoteEvent(StringHash eventType, bool inOrder, const VariantMap& eventData = VariantMap());
    /// Send a remote node event
    void SendRemoteEvent(Node* receiver, StringHash eventType, bool inOrder, const VariantMap& eventData = VariantMap());
    /// Assign scene. On the server, this will cause the client to load it
    void SetScene(Scene* newScene);
    /// Assign identity. Called by Network
    void SetIdentity(const VariantMap& identity);
    /// Set new controls. Moves the current controls as previous
    void SetControls(const Controls& newControls);
    /// Set the connection pending status. Called by Network
    void SetConnectPending(bool connectPending);
    /// Disconnect. If wait time is non-zero, will block while waiting for disconnect to finish
    void Disconnect(int waitMSec = 0);
    /// Send scene update messages. Called by Network
    void SendServerUpdate();
    /// Send latest controls from the client. Called by Network
    void SendClientUpdate();
    /// Send queued remote events. Called by Network
    void SendQueuedRemoteEvents();
    /// Process pending latest data for nodes and components
    void ProcessPendingLatestData();
    /// Process a LoadScene message from the server. Called by Network
    void ProcessLoadScene(int msgID, MemoryBuffer& msg);
    /// Process a SceneChecksumError message from the server. Called by Network
    void ProcessSceneChecksumError(int msgID, MemoryBuffer& msg);
    /// Process a scene update message from the server. Called by Network
    void ProcessSceneUpdate(int msgID, MemoryBuffer& msg);
    /// Process package download related messages. Called by Network
    void ProcessPackageDownload(int msgID, MemoryBuffer& msg);
    /// Process an Identity message from the client. Called by Network
    void ProcessIdentity(int msgID, MemoryBuffer& msg);
    /// Process a Controls message from the client. Called by Network
    void ProcessControls(int msgID, MemoryBuffer& msg);
    /// Process a SceneLoaded message from the client. Called by Network
    void ProcessSceneLoaded(int msgID, MemoryBuffer& msg);
    /// Process a remote event message from the client or server. Called by Network
    void ProcessRemoteEvent(int msgID, MemoryBuffer& msg);
    
    /// Return the kNet message connection
    kNet::MessageConnection* GetMessageConnection() const;
    /// Return client identity
    const VariantMap& GetIdentity() const { return identity_; }
    /// Return the scene used by this connection
    Scene* GetScene() const;
    /// Return the client controls of this connection
    const Controls& GetControls() const { return controls_; }
    /// Return the previous client controls of this connection
    const Controls& GetPreviousControls() const { return previousControls_; }
    /// Return whether is a client connection
    bool IsClient() const { return isClient_; }
    /// Return whether is fully connected
    bool IsConnected() const;
    /// Return whether connection is pending
    bool IsConnectPending() const { return connectPending_; }
    /// Return whether the scene is loaded and ready to receive updates from network
    bool IsSceneLoaded() const { return sceneLoaded_; }
    /// Return remote address
    String GetAddress() const;
    /// Return remote port
    unsigned short GetPort() const;
    /// Return an address:port string
    String ToString() const;
    /// Return number of package downloads remaining
    unsigned GetNumDownloads() const;
    /// Return name of current package download, or empty if no downloads
    const String& GetDownloadName() const;
    /// Return progress of current package download, or 1.0 if no downloads
    float GetDownloadProgress() const;
    
private:
    /// Handle scene loaded event
    void HandleAsyncLoadFinished(StringHash eventType, VariantMap& eventData);
    /// Process a node for sending a network update. Recurses to process depended on node(s) first
    void ProcessNode(Node* node);
    /// Process a node that the client had not yet received
    void ProcessNewNode(Node* node);
    /// Process a node that the client has already received
    void ProcessExistingNode(Node* node);
    /// Initiate a package download
    void RequestPackage(const String& name, unsigned fileSize, unsigned checksum);
    /// Send an error reply for a package download
    void SendPackageError(const String& name);
    /// Handle scene load failure on the server or client
    void OnSceneLoadFailed();
    /// Handle a package download failure on the client
    void OnPackageDownloadFailed(const String& name);
    /// Handle all packages loaded successfully. Also called directly on MSG_LOADSCENE if there are none
    void OnPackagesReady();
    
    /// kNet message connection
    kNet::SharedPtr<kNet::MessageConnection> connection_;
    /// Identity map
    VariantMap identity_;
    /// Scene
    WeakPtr<Scene> scene_;
    /// Last sent state of the scene for network replication
    Map<unsigned, NodeReplicationState> sceneState_;
    /// Pending latest data for not yet received nodes
    Map<unsigned, PODVector<unsigned char> > nodeLatestData_;
    /// Pending latest data for not yet received components
    Map<unsigned, PODVector<unsigned char> > componentLatestData_;
    /// Queued remote events
    Vector<RemoteEvent> remoteEvents_;
    /// Delta update bits
    PODVector<unsigned char> deltaUpdateBits_;
    /// Node's changed user variables
    HashSet<ShortStringHash> changedVars_;
    /// Already processed nodes during a replication update
    HashSet<Node*> processedNodes_;
    /// Preallocated variants of correct type per networked object class
    Map<ShortStringHash, Vector<Variant> > classCurrentState_;
    /// Waiting or ongoing package file downloads
    Map<StringHash, PackageDownload> downloads_;
    /// Scene file to load once all packages (if any) have been downloaded
    String sceneFileName_;
    /// Reused message buffer
    VectorBuffer msg_;
    /// Current controls
    Controls controls_;
    /// Previous controls
    Controls previousControls_;
    /// Update frame number
    unsigned frameNumber_;
    /// Client flag
    bool isClient_;
    /// Connection pending flag
    bool connectPending_;
    /// Scene loaded flag
    bool sceneLoaded_;
};
