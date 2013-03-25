// Urho3D editor hierarchy window handling

const int ITEM_NONE = 0;
const int ITEM_NODE = 1;
const int ITEM_COMPONENT = 2;
const uint NO_ITEM = M_MAX_UNSIGNED;

Window@ sceneWindow;
ListView@ hierarchyList;

bool suppressSceneChanges = false;

void CreateSceneWindow()
{
    if (sceneWindow !is null)
        return;

    sceneWindow = ui.LoadLayout(cache.GetResource("XMLFile", "UI/EditorSceneWindow.xml"));
    hierarchyList = sceneWindow.GetChild("NodeList");
    ui.root.AddChild(sceneWindow);
    int height = Min(ui.root.height - 60, 500);
    sceneWindow.SetSize(300, height);
    sceneWindow.SetPosition(20, 40);
    sceneWindow.opacity = uiMaxOpacity;
    sceneWindow.BringToFront();
    UpdateSceneWindow();

    DropDownList@ newNodeList = sceneWindow.GetChild("NewNodeList", true);
    Array<String> newNodeChoices = {"Replicated", "Local"};
    for (uint i = 0; i < newNodeChoices.length; ++i)
    {
        Text@ choice = Text();
        choice.SetStyle(uiStyle, "FileSelectorFilterText");
        choice.text = newNodeChoices[i];
        newNodeList.AddItem(choice);
    }

    DropDownList@ newComponentList = sceneWindow.GetChild("NewComponentList", true);
    Array<String> componentTypes = GetAvailableComponents();
    for (uint i = 0; i < componentTypes.length; ++i)
    {
        Text@ choice = Text();
        choice.SetStyle(uiStyle, "FileSelectorFilterText");
        choice.text = componentTypes[i];
        newComponentList.AddItem(choice);
    }

    // Set drag & drop target mode on the node list background, which is used to parent nodes back to the root node
    hierarchyList.contentElement.dragDropMode = DD_TARGET;
    hierarchyList.scrollPanel.dragDropMode = DD_TARGET;

    SubscribeToEvent(sceneWindow.GetChild("CloseButton", true), "Released", "HideSceneWindow");
    SubscribeToEvent(sceneWindow.GetChild("ExpandButton", true), "Released", "ExpandCollapseHierarchy");
    SubscribeToEvent(sceneWindow.GetChild("CollapseButton", true), "Released", "ExpandCollapseHierarchy");
    SubscribeToEvent(hierarchyList, "SelectionChanged", "HandleSceneWindowSelectionChange");
    SubscribeToEvent(hierarchyList, "ItemDoubleClicked", "HandleSceneWindowItemDoubleClick");
    SubscribeToEvent(hierarchyList, "UnhandledKey", "HandleSceneWindowKey");
    SubscribeToEvent(newNodeList, "ItemSelected", "HandleCreateNode");
    SubscribeToEvent(newComponentList, "ItemSelected", "HandleCreateComponent");
    SubscribeToEvent("DragDropTest", "HandleDragDropTest");
    SubscribeToEvent("DragDropFinish", "HandleDragDropFinish");
    SubscribeToEvent(editorScene, "NodeAdded", "HandleNodeAdded");
    SubscribeToEvent(editorScene, "NodeRemoved", "HandleNodeRemoved");
    SubscribeToEvent(editorScene, "ComponentAdded", "HandleComponentAdded");
    SubscribeToEvent(editorScene, "ComponentRemoved", "HandleComponentRemoved");
    SubscribeToEvent(editorScene, "NodeNameChanged", "HandleNodeNameChanged");
}

void ShowSceneWindow()
{
    sceneWindow.visible = true;
    sceneWindow.BringToFront();
}

void HideSceneWindow()
{
    sceneWindow.visible = false;
}

void ExpandCollapseHierarchy(StringHash eventType, VariantMap& eventData)
{
    Button@ button = eventData["Element"].GetUIElement();
    bool enable = button.name == "ExpandButton";
    bool all = cast<CheckBox>(sceneWindow.GetChild("AllCheckBox", true)).checked;

    Array<uint> selections = hierarchyList.selections;
    for (uint i = 0; i < selections.length; ++i)
        hierarchyList.Expand(selections[i], enable, all);
}

void EnableExpandCollapseButtons(bool enable)
{
    String[] buttons = { "ExpandButton", "CollapseButton", "AllCheckBox" };
    for (uint i = 0; i < buttons.length; ++i)
    {
        UIElement@ element = sceneWindow.GetChild(buttons[i], true);
        element.active = enable;
        element.children[0].color = enable ? normalTextColor : nonEditableTextColor;
    }
}

void ClearSceneWindow()
{
    if (sceneWindow is null)
        return;

    hierarchyList.RemoveAllItems();
}

void UpdateSceneWindow()
{
    ClearSceneWindow();
    UpdateSceneWindowNode(0, editorScene, null);

    // Re-enable layout update
    hierarchyList.EnableLayoutUpdate();

    // Clear copybuffer when whole window refreshed
    copyBuffer.Clear();
}

uint UpdateSceneWindowNode(uint itemIndex, Node@ node, UIElement@ parentItem)
{
    // Whenever we're updating, disable layout update to optimize speed
    hierarchyList.contentElement.DisableLayoutUpdate();

    // Remove old item if exists
    if (itemIndex < hierarchyList.numItems && (node is null || (hierarchyList.items[itemIndex].vars["Type"].GetInt() == ITEM_NODE &&
        hierarchyList.items[itemIndex].vars["NodeID"].GetUInt() == node.id)))
        hierarchyList.RemoveItem(itemIndex);
    if (node is null)
    {
        hierarchyList.contentElement.EnableLayoutUpdate();
        hierarchyList.contentElement.UpdateLayout();
        return itemIndex;
    }

    Text@ text = Text();
    text.SetStyle(uiStyle, "FileSelectorListText");
    text.vars["Type"] = ITEM_NODE;
    text.vars["NodeID"] = node.id;
    text.text = GetNodeTitle(node);

    BorderImage@ icon = BorderImage();
    icon.texture = cache.GetResource("Texture2D", "Textures/UrhoDecal.dds");
    icon.blendMode = BLEND_ADD;
    text.AddChild(icon);

    // Nodes can be moved by drag and drop. The root node (scene) can not.
    if (node.typeName == "Node")
    {
        text.dragDropMode = DD_SOURCE_AND_TARGET;
        icon.color = Color(1, 1, 0);
    }
    else
    {
        text.dragDropMode = DD_TARGET;
        icon.color = Color(1, 0, 0);
    }

    hierarchyList.InsertItem(itemIndex, text, parentItem);

    // Advance the index for the child components and/or nodes
    if (itemIndex == M_MAX_UNSIGNED)
        itemIndex = hierarchyList.numItems;
    else
        ++itemIndex;

    // Get the indent level after the item is inserted
    icon.indent = text.indent - 1;
    icon.SetFixedSize(text.indentWidth, 16);

    // Update components first
    for (uint i = 0; i < node.numComponents; ++i)
    {
        Component@ component = node.components[i];
        AddComponentToSceneWindow(component, itemIndex++, text);
    }

    // Then update child nodes recursively
    for (uint i = 0; i < node.numChildren; ++i)
    {
        Node@ childNode = node.children[i];
        itemIndex = UpdateSceneWindowNode(itemIndex, childNode, text);
    }

    // Re-enable layout update (and do manual layout) now
    hierarchyList.contentElement.EnableLayoutUpdate();
    hierarchyList.contentElement.UpdateLayout();

    return itemIndex;
}

void UpdateSceneWindowNodeOnly(uint itemIndex, Node@ node)
{
    Text@ text = hierarchyList.items[itemIndex];
    if (text is null)
        return;
    text.text = GetNodeTitle(node);
}

void UpdateSceneWindowNode(Node@ node)
{
    // In case of node's parent is not found in the hierarchy list then the node will inserted at the root level, but it should not happen
    UpdateSceneWindowNode(GetNodeListIndex(node), node, hierarchyList.items[GetNodeListIndex(node.parent)]);
}

void UpdateSceneWindowNodeOnly(Node@ node)
{
    uint index = GetNodeListIndex(node);
    UpdateSceneWindowNodeOnly(index, node);
}

void AddComponentToSceneWindow(Component@ component, uint compItemIndex, UIElement@ parentItem)
{
    Text@ text = Text();
    text.SetStyle(uiStyle, "FileSelectorListText");
    text.vars["Type"] = ITEM_COMPONENT;
    text.vars["NodeID"] = component.node.id;
    text.vars["ComponentID"] = component.id;
    text.text = GetComponentTitle(component);
    
    BorderImage@ icon = BorderImage();
    icon.texture = cache.GetResource("Texture2D", "Textures/Mushroom.dds");
    text.AddChild(icon);

    hierarchyList.InsertItem(compItemIndex, text, parentItem);
    icon.indent = text.indent - 1;
    icon.SetFixedSize(text.indentWidth - 4, 14);
}

uint GetNodeListIndex(Node@ node)
{
    if (node is null)
        return NO_ITEM;

    uint numItems = hierarchyList.numItems;
    uint nodeID = node.id;

    for (uint i = 0; i < numItems; ++i)
    {
        UIElement@ item = hierarchyList.items[i];
        if (item.vars["Type"].GetInt() == ITEM_NODE && item.vars["NodeID"].GetUInt() == nodeID)
            return i;
    }

    return NO_ITEM;
}

uint GetNodeListIndex(Node@ node, uint startPos)
{
    if (node is null)
        return NO_ITEM;

    uint numItems = hierarchyList.numItems;
    uint nodeID = node.id;

    for (uint i = startPos; i < numItems; --i)
    {
        UIElement@ item = hierarchyList.items[i];
        if (item.vars["Type"].GetInt() == ITEM_NODE && item.vars["NodeID"].GetInt() == int(nodeID))
            return i;
    }

    return NO_ITEM;
}

Node@ GetListNode(uint index)
{
    UIElement@ item = hierarchyList.items[index];
    if (item is null)
        return null;

    return editorScene.GetNode(item.vars["NodeID"].GetUInt());
}

Component@ GetListComponent(uint index)
{
    UIElement@ item = hierarchyList.items[index];
    return GetListComponent(item);
}

Component@ GetListComponent(UIElement@ item)
{
    if (item is null)
        return null;

    if (item.vars["Type"].GetInt() != ITEM_COMPONENT)
        return null;

    return editorScene.GetComponent(item.vars["ComponentID"].GetUInt());
}

uint GetComponentListIndex(Component@ component)
{
    if (component is null)
        return NO_ITEM;

    uint numItems = hierarchyList.numItems;
    for (uint i = 0; i < numItems; ++i)
    {
        UIElement@ item = hierarchyList.items[i];
        if (item.vars["Type"].GetInt() == ITEM_COMPONENT && item.vars["ComponentID"].GetUInt() == component.id)
            return i;
    }

    return NO_ITEM;
}

int GetNodeIndent(Node@ node)
{
    int indent = 0;
    for (;;)
    {
        if (node.parent is null)
            break;
        ++indent;
        node = node.parent;
    }
    return indent;
}

String GetNodeTitle(Node@ node)
{
    String idStr;
    if (node.id >= FIRST_LOCAL_ID)
        idStr = "Local " + String(node.id - FIRST_LOCAL_ID);
    else
        idStr = String(node.id);

    if (node.name.empty)
        return node.typeName + " (" + idStr + ")";
    else
        return node.name + " (" + idStr + ")";
}

String GetComponentTitle(Component@ component)
{
    String localStr;
    if (component.id >= FIRST_LOCAL_ID)
        localStr = " (Local)";

    return component.typeName + localStr;
}

void SelectNode(Node@ node, bool multiselect)
{
    if (node is null && !multiselect)
    {
        hierarchyList.ClearSelection();
        return;
    }

    uint nodeItem = GetNodeListIndex(node);

    // Go in the parent chain up to make sure the chain is expanded
    for (;;)
    {
        Node@ parent = node.parent;
        if (node is editorScene || parent is null)
            break;
        node = parent;
    }
    
    uint numItems = hierarchyList.numItems;
    uint parentItem = GetNodeListIndex(node);

    if (nodeItem < numItems)
    {
        // Expand the node chain now
        if (!multiselect || !hierarchyList.IsSelected(nodeItem))
        {
            if (parentItem < numItems)
                hierarchyList.Expand(parentItem, true);
            hierarchyList.Expand(nodeItem, true);
        }
        // This causes an event to be sent, in response we set the node/component selections, and refresh editors
        if (!multiselect)
            hierarchyList.selection = nodeItem;
        else
            hierarchyList.ToggleSelection(nodeItem);
    }
    else
    {
        if (!multiselect)
            hierarchyList.ClearSelection();
    }
}

void SelectComponent(Component@ component, bool multiselect)
{
    if (component is null && !multiselect)
    {
        hierarchyList.ClearSelection();
        return;
    }
    
    Node@ node = component.node;
    if (node is null && !multiselect)
    {
        hierarchyList.ClearSelection();
        return;
    }

    uint nodeItem = GetNodeListIndex(node);
    uint componentItem = GetComponentListIndex(component);
    
    // Go in the parent chain up to make sure the chain is expanded
    for (;;)
    {
        Node@ parent = node.parent;
        if (node is editorScene || parent is null)
            break;
        node = parent;
    }

    uint numItems = hierarchyList.numItems;
    uint parentItem = GetNodeListIndex(node);

    if (parentItem >= hierarchyList.numItems && !multiselect)
    {
        hierarchyList.ClearSelection();
        return;
    }

    if (nodeItem < numItems && componentItem < numItems)
    {
        // Expand the node chain now
        if (!multiselect || !hierarchyList.IsSelected(componentItem))
        {
            if (parentItem < numItems)
                hierarchyList.Expand(parentItem, true);
            hierarchyList.Expand(nodeItem, true);
        }
        // This causes an event to be sent, in response we set the node/component selections, and refresh editors
        if (!multiselect)
            hierarchyList.selection = componentItem;
        else
            hierarchyList.ToggleSelection(componentItem);
    }
    else
    {
        if (!multiselect)
            hierarchyList.ClearSelection();
    }
}

void HandleSceneWindowSelectionChange()
{
    if (inSelectionModify)
        return;
    
    ClearSelection();

    Array<uint> indices = hierarchyList.selections;

    // Enable Expand/Collapse button when there is selection
    EnableExpandCollapseButtons(indices.length > 0);
    
    for (uint i = 0; i < indices.length; ++i)
    {
        uint index = indices[i];
        UIElement@ item = hierarchyList.items[index];
        int type = item.vars["Type"].GetInt();
        if (type == ITEM_COMPONENT)
        {
            Component@ comp = GetListComponent(index);
            if (comp !is null)
                selectedComponents.Push(comp);
        }
        else if (type == ITEM_NODE)
        {
            Node@ node = GetListNode(index);
            if (node !is null)
                selectedNodes.Push(node);
        }
    }

    // If only one node selected, use it for editing
    if (selectedNodes.length == 1)
        editNode = selectedNodes[0];

    // If selection contains only components, and they have a common node, use it for editing
    if (selectedNodes.empty && !selectedComponents.empty)
    {
        Node@ commonNode;
        for (uint i = 0; i < selectedComponents.length; ++i)
        {
            if (i == 0)
                commonNode = selectedComponents[i].node;
            else
            {
                if (selectedComponents[i].node !is commonNode)
                    commonNode = null;
            }
        }
        editNode = commonNode;
    }
    
    // Now check if the component(s) can be edited. If many selected, must have same type or have same edit node
    if (!selectedComponents.empty)
    {
        if (editNode is null)
        {
            ShortStringHash compType = selectedComponents[0].type;
            bool sameType = true;
            for (uint i = 1; i < selectedComponents.length; ++i)
            {
                if (selectedComponents[i].type != compType)
                {
                    sameType = false;
                    break;
                }
            }
            if (sameType)
                editComponents = selectedComponents;
        }
        else
        {
            editComponents = selectedComponents;
            numEditableComponentsPerNode = selectedComponents.length;
        }
    }
    
    // If just nodes selected, and no components, show as many matching components for editing as possible
    if (!selectedNodes.empty && selectedComponents.empty && selectedNodes[0].numComponents > 0)
    {
        uint count = 0;
        for (uint j = 0; j < selectedNodes[0].numComponents; ++j)
        {
            ShortStringHash compType = selectedNodes[0].components[j].type;
            bool sameType = true;
            for (uint i = 1; i < selectedNodes.length; ++i)
            {
                if (selectedNodes[i].numComponents <= j || selectedNodes[i].components[j].type != compType)
                {
                    sameType = false;
                    break;
                }
            }

            if (sameType)
            {
                ++count;
                for (uint i = 0; i < selectedNodes.length; ++i)
                    editComponents.Push(selectedNodes[i].components[j]);
            }
        }
        if (count > 1)
            numEditableComponentsPerNode = count;
    }

    if (selectedNodes.empty && editNode !is null)
        editNodes.Push(editNode);
    else
    {
        editNodes = selectedNodes;
        
        // Ensure the first one in array is not the scene node because the first node is used as template for attribute editing
        if (editNodes.length > 1 && editNodes[0] is editorScene)
        {
            editNodes.Erase(0);
            editNodes.Push(editorScene);
        }
    }

    PositionGizmo();
    UpdateNodeWindow();
}

void HandleSceneWindowItemDoubleClick(StringHash eventType, VariantMap& eventData)
{
    uint index = eventData["Selection"].GetUInt();
    hierarchyList.ToggleExpand(index);
}

void HandleSceneWindowKey(StringHash eventType, VariantMap& eventData)
{
    int key = eventData["Key"].GetInt();
}

void HandleDragDropTest(StringHash eventType, VariantMap& eventData)
{
    UIElement@ source = eventData["Source"].GetUIElement();
    UIElement@ target = eventData["Target"].GetUIElement();
    eventData["Accept"] = TestSceneWindowElements(source, target);
}

void HandleDragDropFinish(StringHash eventType, VariantMap& eventData)
{
    UIElement@ source = eventData["Source"].GetUIElement();
    UIElement@ target = eventData["Target"].GetUIElement();
    bool accept =  TestSceneWindowElements(source, target);
    eventData["Accept"] = accept;
    if (!accept)
        return;

    Node@ sourceNode = editorScene.GetNode(source.vars["NodeID"].GetUInt());
    Node@ targetNode = editorScene.GetNode(target.vars["NodeID"].GetUInt());

    // If target is null, parent to scene
    if (targetNode is null)
        targetNode = editorScene;

    // Perform the reparenting
    if (!SceneChangeParent(sourceNode, targetNode))
        return;

    // Focus the node at its new position in the list which in turn should trigger a refresh in attribute inspector
    FocusNode(sourceNode);
}

bool TestSceneWindowElements(UIElement@ source, UIElement@ target)
{
    // Test for validity of reparenting by drag and drop
    Node@ sourceNode;
    Node@ targetNode;
    if (source.vars.Contains("NodeID"))
        sourceNode = editorScene.GetNode(source.vars["NodeID"].GetUInt());
    if (target.vars.Contains("NodeID"))
        editorScene.GetNode(target.vars["NodeID"].GetUInt());

    if (sourceNode is null)
        return false;
    if (sourceNode is editorScene)
        return false;
    if (targetNode !is null)
    {
        if (sourceNode.parent is targetNode)
            return false;
        if (targetNode.parent is sourceNode)
            return false;
    }
    
    return true;
}

void FocusNode(Node@ node)
{
    uint index = GetNodeListIndex(node);
    hierarchyList.selection = index;
}

void FocusComponent(Component@ component)
{
    uint index = GetComponentListIndex(component);
    hierarchyList.selection = index;
}

void HandleCreateNode(StringHash eventType, VariantMap& eventData)
{
    DropDownList@ list = eventData["Element"].GetUIElement();
    uint mode = list.selection;
    if (mode >= list.numItems)
        return;

    Node@ newNode = editorScene.CreateChild("", mode == 0 ? REPLICATED : LOCAL);
    // Set the new node a certain distance from the camera
    newNode.position = GetNewNodePosition();

    FocusNode(newNode);
}

void HandleCreateComponent(StringHash eventType, VariantMap& eventData)
{
    if (editNode is null)
        return;

    DropDownList@ list = eventData["Element"].GetUIElement();
    Text@ text = list.selectedItem;
    if (text is null)
        return;

    // If this is the root node, do not allow to create duplicate scene-global components
    if (editNode is editorScene && CheckForExistingGlobalComponent(editNode, text.text))
        return;

    // For now, make a local node's all components local
    /// \todo Allow to specify the createmode
    Component@ newComponent = editNode.CreateComponent(text.text, editNode.id < FIRST_LOCAL_ID ? REPLICATED : LOCAL);

    FocusComponent(newComponent);
}

void CreateBuiltinObject(const String& name)
{
    Node@ newNode = editorScene.CreateChild(name, REPLICATED);
    // Set the new node a certain distance from the camera
    newNode.position = GetNewNodePosition();

    StaticModel@ object = newNode.CreateComponent("StaticModel");
    object.model = cache.GetResource("Model", "Models/" + name + ".mdl");

    FocusNode(newNode);
}

bool CheckSceneWindowFocus()
{
    // When we do scene operations based on key shortcuts, make sure either the 3D scene or the node list is focused,
    // not for example a file selector
    if (ui.focusElement is hierarchyList || ui.focusElement is null)
        return true;
    else
        return false;
}

bool CheckForExistingGlobalComponent(Node@ node, const String&in typeName)
{
    if (typeName != "Octree" && typeName != "PhysicsWorld" && typeName != "DebugRenderer")
        return false;
    else
        return node.HasComponent(typeName);
}

void HandleNodeAdded(StringHash eventType, VariantMap& eventData)
{
    if (suppressSceneChanges)
        return;

    Node@ node = eventData["Node"].GetNode();
    UpdateSceneWindowNode(node);
}

void HandleNodeRemoved(StringHash eventType, VariantMap& eventData)
{
    if (suppressSceneChanges)
        return;

    Node@ node = eventData["Node"].GetNode();
    uint index = GetNodeListIndex(node);
    UpdateSceneWindowNode(index, null, null);
}

void HandleComponentAdded(StringHash eventType, VariantMap& eventData)
{
    if (suppressSceneChanges)
        return;

    Node@ node = eventData["Node"].GetNode();
    UpdateSceneWindowNode(node);
}

void HandleComponentRemoved(StringHash eventType, VariantMap& eventData)
{
    if (suppressSceneChanges)
        return;

    Component@ component = eventData["Component"].GetComponent();
    uint index = GetComponentListIndex(component);
    if (index != NO_ITEM)
    {
        ListView@ list = sceneWindow.GetChild("NodeList", true);
        list.RemoveItem(index);
    }
}

void HandleNodeNameChanged(StringHash eventType, VariantMap& eventData)
{
    if (suppressSceneChanges)
        return;
    
    Node@ node = eventData["Node"].GetNode();
    UpdateSceneWindowNodeOnly(node);
}