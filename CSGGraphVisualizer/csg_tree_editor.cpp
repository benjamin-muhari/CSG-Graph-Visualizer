#include "application.h"
#include "ImguiNodeEditor/imgui_node_editor.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui_internal.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "nlohmann-json/json.hpp"
#include "ImFileDialog/ImFileDialog.h"
#include "performance.h"

#include "CodeGen/expr.h"
#include "CodeGen/codegen.h"
#include "CodeGen/Material2.h"
#include "CodeGen/Models/models.h"
#include "CodeGen/util.h"

#include <fstream>
#include <string>
#include <vector>

// TODO: DELETE
#include <iostream>

#define GLM_FORCE_SWIZZLE

//namespace CsgTreeEditor {

namespace ed = ax::NodeEditor;

// Json parsing
using json = nlohmann::json;
using namespace nlohmann::literals;

// TODO: Most likely not needed
enum class PinType
{
    Flow,
    Bool,
    Int,
    Float,
    String,
    Object,
    Function,
    Delegate,
    CsgUniversal
};

enum class PinKind
{
    Output,
    Input
};

struct CsgNode;

struct CsgPin
{
    ed::PinId   ID;
    ::Expr<CsgNode>*  Node;
    // TODO: probably not needed, maybe logic around it
    std::string Name;
    // TODO: maybe not needed, maybe logic around it
    PinType     Type;
    PinKind     Kind;

    CsgPin(int id, const char* name, PinType type = PinType::CsgUniversal) :
        ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input) {}
    // ^ set pinkind when building node
};

struct CsgNode
{
    ed::NodeId id;
    std::vector<CsgPin> InputPins;
    std::vector<CsgPin> OutputPins;

    int material;

    CsgNode(int _id, int _material = 0) :
        id(_id), material(_material) {}
    CsgNode() : CsgNode(0, 0) {}
};

struct CsgLink
{
    ed::LinkId ID;

    CsgPin* StartPin;
    CsgPin* EndPin;

    ImColor Color;

    CsgLink(ed::LinkId id, CsgPin* startPin, CsgPin* endPin) :
        ID(id), StartPin(startPin), EndPin(endPin), Color(255, 255, 255)
    {
    }
};

static Expr<CsgNode>* selected_root;
static Expr<CsgNode>* everything_root = nullptr;
static bool graph_changed;
static bool render_all = true; // TODO: add checkbox selector, default:false
static bool generate_clicked; // TODO: change according to final logic generation/autogeneration logic
static bool reloaded_editor = false;
static std::string savefile = "export.json";
static int material_array_size = 7;

// Save mouse position at the time of rightclick, because its weird if querried inside some imgui stuff
static ImVec2 newNodePosition;
// TODO .h
static void ConnectCsgNodes(CsgPin* startPin, CsgPin* endPin);
static void BuildCsgNode(Expr<CsgNode>* node);

// TODO: Test if need all, try delete ?
#include <map>
#include <algorithm> // ?
#include <utility> // ?

//asd;
// TODO: delete
//using namespace ax;
//
//  Deleted functionality 
//void DrawPinIcon(const Pin& pin, bool connected, int alpha);
// 
//s_HeaderBackground = Application_LoadTexture("Data/BlueprintBackground.png");
//s_SaveIcon = Application_LoadTexture("Data/ic_save_white_24dp.png");
//s_RestoreIcon = Application_LoadTexture("Data/ic_restore_white_24dp.png");
//
//releaseTexture(s_RestoreIcon);
//releaseTexture(s_SaveIcon);
//releaseTexture(s_HeaderBackground);
//
//int saveIconWidth = Application_GetTextureWidth(s_SaveIcon);
//int saveIconHeight = Application_GetTextureWidth(s_SaveIcon);
//int restoreIconWidth = Application_GetTextureWidth(s_RestoreIcon);
//int restoreIconHeight = Application_GetTextureWidth(s_RestoreIcon);
//
//  Works in v1.76, not v1.82wip, check demowindow maybe
//ImGui::BeginHorizontal
//ImGui::Spring();
//ImGui::BeginVertical
//
//static ImTextureID          s_HeaderBackground = nullptr;
//static ImTextureID          s_SampleImage = nullptr;
//static ImTextureID          s_SaveIcon = nullptr;
//static ImTextureID          s_RestoreIcon = nullptr;
//
//static inline ImRect ImGui_GetItemRect()
//static inline ImRect ImRect_Expanded(const ImRect& rect, float x, float y)
//




// TODO: Delete all node logic
enum class NodeType
{
    Blueprint,
    Simple,
    Tree,
    Comment,
    Houdini
};

struct Node;

struct Pin
{
    ed::PinId   ID;
    ::Node*     Node;
    std::string Name;
    PinType     Type;
    PinKind     Kind;

    Pin(int id, const char* name, PinType type) :
        ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input)
    {
    }
};

struct Node
{
    ed::NodeId ID;
    std::string Name;
    std::vector<Pin> Inputs;
    std::vector<Pin> Outputs;
    ImColor Color;
    NodeType Type;
    ImVec2 Size;

    std::string State;
    std::string SavedState;

    Node(int id, const char* name, ImColor color = ImColor(255, 255, 255)) :
        ID(id), Name(name), Color(color), Type(NodeType::Blueprint), Size(0, 0)
    {
    }
};

// TODO: Delete s_Nodes eventually, own logic
static std::vector<Node>            s_Nodes;


static ed::EditorContext* m_Editor = nullptr;

static std::vector<CsgLink>         s_Links;
static std::vector<Expr<CsgNode>*>  s_roots;


struct NodeIdLess
{
    bool operator()(const ed::NodeId& lhs, const ed::NodeId& rhs) const
    {
        return lhs.AsPointer() < rhs.AsPointer();
    }
};

static const float                              s_TouchTime = 1.0f;
static std::map<ed::NodeId, float, NodeIdLess>  s_NodeTouchTime;

// TODO: Maybe not here lol
static int s_NextId = 1;
static int GetNextId()
{
    return s_NextId++;
}

static void ResetIdCounter()
{
    s_NextId = 1;
}

static void TouchNode(ed::NodeId id)
{
    s_NodeTouchTime[id] = s_TouchTime;
}

static float GetTouchProgress(ed::NodeId id)
{
    auto it = s_NodeTouchTime.find(id);
    if (it != s_NodeTouchTime.end() && it->second > 0.0f)
        return (s_TouchTime - it->second) / s_TouchTime;
    else
        return 0.0f;
}

static void UpdateTouch()
{
    const auto deltaTime = ImGui::GetIO().DeltaTime;
    for (auto& entry : s_NodeTouchTime)
    {
        if (entry.second > 0.0f)
            entry.second -= deltaTime;
    }
}

static CsgLink* FindLink(ed::LinkId id)
{
    for (auto& link : s_Links)
        if (link.ID == id)
            return &link;

    return nullptr;
}

// TODO: Delete eventually, own logic
static Node* FindNode(ed::NodeId id)
{
    for (auto& node : s_Nodes)
        if (node.ID == id)
            return &node;

    return nullptr;
}

// TODO: Delete eventually, own logic
static Pin* FindPin(ed::PinId id)
{
    if (!id)
        return nullptr;

    for (auto& node : s_Nodes)
    {
        for (auto& pin : node.Inputs)
            if (pin.ID == id)
                return &pin;

        for (auto& pin : node.Outputs)
            if (pin.ID == id)
                return &pin;
    }

    return nullptr;
}

template<typename Fields>
struct FindNodeVisitor
{
    ed::NodeId id;
    Expr<Fields>* self;

    Expr<Fields>* operator()(Box<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        return nullptr;
    }
    Expr<Fields>* operator()(Sphere<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        return nullptr;
    }
    Expr<Fields>* operator()(Cylinder<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        return nullptr;
    }
    Expr<Fields>* operator()(PlaneXZ<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        return nullptr;
    }
    //--------------------------------------------------------------
    //--------------------------------------------------------------
    Expr<Fields>* operator()(Move<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        if (expr.a != nullptr)
        {
            self = expr.a;
            // TODO: potential memory leak, not really tho
            return visit(*this, *expr.a);
        }

        return nullptr;
    }
    Expr<Fields>* operator()(Rotate<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        if (expr.a != nullptr)
        {
            self = expr.a;
            return visit(*this, *expr.a);
        }
        return nullptr;
    }
    Expr<Fields>* operator()(Offset<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        if (expr.a != nullptr)
        {
            self = expr.a;
            return visit(*this, *expr.a);
        }
        return nullptr;
    }
    Expr<Fields>* operator()(Invert<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        if (expr.a != nullptr)
        {
            self = expr.a;
            return visit(*this, *expr.a);
        }
        return nullptr;
    }
    //--------------------------------------------------------------
    //--------------------------------------------------------------
    Expr<Fields>* operator()(Intersect<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        else
        {
            Expr<Fields>* result;
            for (auto& leaf : expr.a)
            {
                self = leaf;
                result = visit(*this, *leaf);
                if (result != nullptr)
                    return result;
            }
            // return null if pin not found on any leaves
            return nullptr;
        }
    }
    Expr<Fields>* operator()(Union<Fields>& expr)
    {
        if (expr.id == id)
            return self;
        else
        {
            Expr<Fields>* result;
            for (auto& leaf : expr.a)
            {
                self = leaf;
                result = visit(*this, *leaf);
                if (result != nullptr)
                    return result;
            }
            // return null if pin not found on any leaves
            return nullptr;
        }
    }
};

static void SelectRootNode()
{
    std::vector<ed::NodeId> selectedNodes;
    selectedNodes.resize(ed::GetSelectedObjectCount());
    int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
    selectedNodes.resize(nodeCount);

    if (selectedNodes.size() == 0)
        return;

    ed::NodeId first_selected = selectedNodes[0];
    for (auto& root : s_roots)
    {
        selected_root = std::visit(FindNodeVisitor<CsgNode>{ first_selected, root }, *root);
        if (selected_root != nullptr)
            return;
    }
    selected_root = nullptr;
}

static void SelectAllRoots()
{
    if (!everything_root)
        everything_root = new Expr<CsgNode>{ Union<CsgNode>{CsgNode{ GetNextId() }, s_roots} };
    else
        std::get<Union<CsgNode>>(*everything_root).a = s_roots;

    selected_root = everything_root;
}

static bool GenerateSdf();

static void AutoGenerateSdf()
{
    SelectAllRoots();
    GenerateSdf();
}

static void ClickGenerateSdf()
{
    SelectRootNode();
    // TODO delete, ^uncomment & use
    //SelectAllRoots();
    generate_clicked = GenerateSdf();
}

static void PerfTestGenerateSdf(int count)
{
    selected_root = new Expr<CsgNode>{ Union<CsgNode>{CsgNode{ GetNextId() },s_roots} };

    for (int i = 0; i < count; i++)
        std::get<Union<CsgNode>>(*selected_root).a.emplace_back(sphere((float)i, CsgNode{ GetNextId() }));

    GenerateSdf();
    delete selected_root;
    // TODO: delete union members, memory leak currently
}

static bool GenerateSdf()
{
    if (selected_root == nullptr)
        return false;

    const std::string glsl_prefix = "// Start of generated GLSL code\n";
    const std::string glsl_postfix = "// End of generated GLSL code\n";

    //std::ofstream kernel("sdf_editortest.cpp");
    //std::ofstream kernel("Shaders/gen_raytrace.frag");
    //kernel << glsl_prefix << sdf(*selected_root) << "\n" << material2(*selected_root) << glsl_postfix;

    //// TODO: Writes char by char, no lines, maybe faster if fixed
    //// TODO: open/close?
    //std::ifstream prefix_file("Shaders/raytrace.frag.template.prefix");
    //std::ifstream postfix_file("Shaders/raytrace.frag.template.postfix");
    //std::ofstream result("Shaders/gen_raytrace.frag");
    //char ch;
    //while (!prefix_file.eof())
    //{
    //    prefix_file >> ch;
    //    result << ch;
    //}

    //result << glsl_prefix << sdf(*selected_root) << "\n" << material2(*selected_root) << glsl_postfix;    
    //
    //while (!postfix_file.eof())
    //{
    //    postfix_file >> ch;
    //    result << ch;
    //}

    std::string buff; //str1 for fetching string line from file 1 and str2 for fetching string from file2

    std::ifstream prefix_file("Shaders/raytrace.frag.template.prefix");
    std::ifstream postfix_file("Shaders/raytrace.frag.template.postfix");
    std::ofstream result("Shaders/gen_raytrace.frag");

    if (GLOBAL_spirv_precompile)
    {
        std::string spirv_prefix = R"(#version 460

float sphere(vec3 p, float r);
float box(vec3 p, vec3 b);

// Infinite cylinders
float cylinderZ(vec3 p, float r);
float cylinderY(vec3 p, float r);

// Finite cylinders
float cylinderZ(vec3 p, vec2 h);
float cylinderY(vec3 p, vec2 h);

// Material Value struct fwd declare
struct Value {float d; int mat;};
struct Material {
    vec3  color;        // [0, 1/pi]    reflective color
    float roughness;    // [0, 7]       shininess
    vec3 emission;		// [0, inf]     light emitting surface if nonzero
    float metalness;    // [0.02, 0.05] for non-metals, [0.6, 0.9] for metals
};

Material colors[7];)";

        result << spirv_prefix << "\n" << glsl_prefix << sdf(*selected_root) << "\n" << material2(*selected_root) << glsl_postfix;
    }
    else
    {
        while (std::getline(prefix_file, buff)) {
            result << buff;
            result << "\n";
        }
        result << "\n" << glsl_prefix << sdf(*selected_root) << "\n" << material2(*selected_root) << glsl_postfix;
        while (std::getline(postfix_file, buff)) {
            result << buff;
            result << "\n";
        }
    }

    // TODO: where to put; works already, but more logical place
    graph_changed = true;
    // dont delete since visitor returning &self now instead of new Expr
    //delete selected_root;

    // TODO: nicer code/comment, delete elsewhere
    //if (everything_root)
    //{
    //    delete everything_root;
    //    everything_root = nullptr
    //}

    // Dont deselect the root TODO: maybe change/better
    //selected_root = nullptr;
    //ed::ClearSelection();

    return true;
}

namespace glm {
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(glm::vec3, x, y, z)

    void to_json(json& j, const glm::mat3& m)
    {
        j = json{
            {"x0", m[0].x}, {"x1", m[1].x}, {"x2", m[2].x},
            {"y0", m[0].y}, {"y1", m[1].y}, {"y2", m[2].y},
            {"z0", m[0].z}, {"z1", m[1].z}, {"z2", m[2].z},
        };
    }

    void from_json(const json& j, glm::mat3& m)
    {
        float x0, x1, x2, y0, y1, y2, z0, z1, z2;
        j.at("x0").get_to(x0);
        j.at("x1").get_to(x1);
        j.at("x2").get_to(x2);

        j.at("y0").get_to(y0);
        j.at("y1").get_to(y1);
        j.at("y2").get_to(y2);

        j.at("z0").get_to(z0);
        j.at("z1").get_to(z1);
        j.at("z2").get_to(z2);
        // glm:mat3 uses coloumn-wise assignemnt order
        m = glm::mat3(x0, y0, z0, x1, y1, z1, x2, y2, z2);
    }
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Box<CsgNode>, material, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Sphere<CsgNode>, material, r)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Cylinder<CsgNode>, material, dir, x) // save y manually, inf causes exception
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlaneXZ<CsgNode>, material)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Move<CsgNode>, material, v)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rotate<CsgNode>, material, m, ui_rotation_type, ui_v,
                                    ui_x_degree, ui_y_degree, ui_z_degree, ui_vec_radian)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Offset<CsgNode>, material, r)

//// Dont need to save any node data from these
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Invert<CsgNode>, material)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Union<CsgNode>, material)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Intersect<CsgNode>, material)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec2, x, y)

enum class CsgNodeType
{
    Box,
    Sphere,
    Cylinder,
    PlaneXZ,
    Move,
    Rotate,
    Offset,
    Invert,
    Intersect,
    Union
};

template<typename Fields>
struct ExportVisitor
{
    std::vector<json>& jnodes;
    
    void SaveNode(json& jnode, ed::NodeId& nodeid, CsgNodeType type)
    {
        jnode["id"] = nodeid.Get();
        jnode["type"] = type;
        jnode["location"] = ed::GetNodePosition(nodeid);
        jnodes.emplace_back(jnode);
        //(*result)["nodes"]["node:" + std::to_string(expr.id.Get())] = jnode;
        // ^result was type json*, vector better to make list
    }

    void operator()(Box<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::Box);
    }
    void operator()(Sphere<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::Sphere);
    }
    void operator()(Cylinder<Fields>& expr)
    {
        json jnode = expr;
        if (expr.y == std::numeric_limits<float>::infinity())
            jnode["y"] = "inf";
        else
            jnode["y"] = expr.y;
        SaveNode(jnode, expr.id, CsgNodeType::Cylinder);
    }
    void operator()(PlaneXZ<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::PlaneXZ);
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    void operator()(Move<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::Move);
        if (expr.a != nullptr)
            visit(*this, *expr.a);
    }
    void operator()(Rotate<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::Rotate);
        if (expr.a != nullptr)
            visit(*this, *expr.a);
    }
    void operator()(Offset<Fields>& expr)
    {
        json jnode = expr;
        SaveNode(jnode, expr.id, CsgNodeType::Offset);
        if (expr.a != nullptr)
            visit(*this, *expr.a);
    }
    void operator()(Invert<Fields>& expr)
    {
        json jnode;
        SaveNode(jnode, expr.id, CsgNodeType::Invert);
        if (expr.a != nullptr)
            visit(*this, *expr.a);
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    void operator()(Intersect<Fields>& expr)
    {
        json jnode;
        SaveNode(jnode, expr.id, CsgNodeType::Intersect);
        for (auto& leaf : expr.a)
            visit(*this, *leaf);
    }
    void operator()(Union<Fields>& expr)
    {
        json jnode;
        SaveNode(jnode, expr.id, CsgNodeType::Union);
        for (auto& leaf : expr.a)
            visit(*this, *leaf);
    }
};

static void SaveNodes(std::string filename = savefile)
{
    json result;
    std::vector<json> jnodes;
    std::vector<json> jlinks;

    for (auto& root : s_roots)
        std::visit(ExportVisitor<CsgNode>{ jnodes }, *root);
    
    for (auto& link : s_Links)
    {
        ed::NodeId startnodeid = std::visit([](auto& n) { return n.id; }, *link.StartPin->Node);
        ed::NodeId endnodeid = std::visit([](auto& n) { return n.id; }, *link.EndPin->Node);

        // "links" : { "link:id":{ "start_id":"end_id"}, "link:id2":{ "start_id":"end_id"}, ...}
        // Resulting format ^
        //(*result)["links"][std::to_string(link.ID.Get())][std::to_string(startnodeid.Get())] = std::to_string(endnodeid.Get());
        
        // "links" : { "link:id":{ "sid":"start_id","eid":"end_id"}, "link:id2":{ "sid":"start_id","eid":"end_id"}, ...}
        // Resulting format ^
        //result["links"]["link:" + std::to_string(link.ID.Get())]["sid"] = std::to_string(startnodeid.Get());
        //result["links"]["link:" + std::to_string(link.ID.Get())]["eid"] = std::to_string(endnodeid.Get());

        json jlink;
        jlink["startid"] = startnodeid.Get();
        jlink["endid"] = endnodeid.Get();
        jlinks.emplace_back(jlink);
    }

    result["nodes"] = jnodes;
    result["links"] = jlinks;

    std::ofstream fo(filename);
    fo << std::setw(4) << result << std::endl;

    std::cout << "Successfully saved to: \"" << filename << "\"\n";
}

static void LoadNodes(std::string filename = savefile)
{
    //// testing #TODO: DELETE
    //InsertMeasurement("Quickload clicked");
    //if (measurements.size() >= 4)
    //    DumpMeasurements();

    std::ifstream fi(filename);
    json data = json::parse(fi);

    // Reload fresh editor context
    Application_Finalize();
    Application_Initialize();
    graph_changed = true;
    
    std::vector<json> jnodes = data["nodes"];
    std::vector<json> jlinks = data["links"];
    std::map<int, Expr<CsgNode>*> oldId_newNode_map;

    // Load nodes
    for (auto& jnode : jnodes)
    {
        Expr<CsgNode>* node = nullptr;
        CsgNodeType type = jnode["type"];

        switch (type)
        {
        case (CsgNodeType::Box):
        {
            Box<CsgNode> box = jnode;
            box.id = GetNextId();
            node = new Expr<CsgNode>{ box };
            std::get<Box<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Sphere):
        {
            Sphere<CsgNode> sphere = jnode;
            sphere.id = GetNextId();
            node = new Expr<CsgNode>{ sphere };
            std::get<Sphere<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Cylinder):
        {
            Cylinder<CsgNode> cyl = jnode;
            // Manually load y value due to inf
            if (jnode["y"] == "inf")
                cyl.y = std::numeric_limits<float>::infinity();
            else
                cyl.y = jnode["y"].template get<float>();
            cyl.id = GetNextId();
            node = new Expr<CsgNode>{ cyl };
            std::get<Cylinder<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::PlaneXZ):
        {
            PlaneXZ<CsgNode> plane = jnode;
            plane.id = GetNextId();
            node = new Expr<CsgNode>{ plane };
            std::get<PlaneXZ<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Move):
        {
            Move<CsgNode> move = jnode;
            move.id = GetNextId();
            node = new Expr<CsgNode>{ move };
            std::get<Move<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            std::get<Move<CsgNode>>(*node).OutputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Rotate):
        {
            Rotate<CsgNode> rot = jnode;
            rot.id = GetNextId();
            node = new Expr<CsgNode>{ rot };
            std::get<Rotate<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            std::get<Rotate<CsgNode>>(*node).OutputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Offset):
        {
            Offset<CsgNode> off = jnode;
            off.id = GetNextId();
            node = new Expr<CsgNode>{ off };
            std::get<Offset<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            std::get<Offset<CsgNode>>(*node).OutputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Invert):
        {
            Invert<CsgNode> inv; // = jnode;
            inv.id = GetNextId();
            node = new Expr<CsgNode>{ inv };
            std::get<Invert<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            std::get<Invert<CsgNode>>(*node).OutputPins.emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Intersect):
        {
            Intersect<CsgNode> inter; // = jnode;
            inter.id = GetNextId();
            node = new Expr<CsgNode>{ inter };
            std::get<Intersect<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            auto outputs = &std::get<Intersect<CsgNode>>(*node).OutputPins;
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            break;
        }
        case (CsgNodeType::Union):
        {
            Union<CsgNode> uni; // = jnode;
            uni.id = GetNextId();
            node = new Expr<CsgNode>{ uni };
            std::get<Union<CsgNode>>(*node).InputPins.emplace_back(GetNextId(), "<<");
            auto outputs = &std::get<Union<CsgNode>>(*node).OutputPins;
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            outputs->emplace_back(GetNextId(), "<<");
            break;
        }}
        // Save the oldid corresponding to the new node, to load links from savefile
        int oldId = jnode["id"];
        oldId_newNode_map[oldId] = node;
        // TODO: need to do anything more with a node?

        BuildCsgNode(node);
        s_roots.emplace_back(node);
        // Set position in UI
        auto newNodeId = std::visit([](auto& n) {return n.id; }, *node);
        ed::SetNodePosition(newNodeId, jnode["location"].template get<ImVec2>());
    }
    // Load links
    for (auto& jlink : jlinks)
    {
        //std::cout << "\n" << jlink;
        int startNodeId_old = jlink["startid"].template get<int>();
        int endNodeId_old = jlink["endid"].template get<int>();
        //int startNodeId = jlink["startid"].template get<int>();
        //int endNodeId = jlink["endid"].template get<int>();

        auto& startNode = oldId_newNode_map[startNodeId_old];
        auto& endNode = oldId_newNode_map[endNodeId_old];

        CsgPin* startPin = nullptr;
        auto outputPins = std::visit([](auto& n) {return &n.OutputPins; }, *startNode);
        // Union/Intersections can have multiple links, insert into the first EMPTY pin
        if (std::get_if<Union<CsgNode>>(startNode) || std::get_if<Intersect<CsgNode>>(startNode))
        {
            for (auto& pin : *outputPins)
            {
                // Find if the pin is already used for another link
                ed::PinId pinid = pin.ID;
                auto link_from_pin =
                    std::find_if(s_Links.begin(), s_Links.end(),
                        [pinid](auto& link) { return link.StartPin->ID == pinid; });
                // If the pin is empty, use it for creating the next link
                if (link_from_pin == s_Links.end())
                {
                    startPin = &pin;
                    break;
                }
            }
        }
        else
            startPin = &(*outputPins)[0];

        CsgPin* endPin = std::visit([](auto& n) {return &(n.InputPins[0]); }, *endNode);

        ConnectCsgNodes(startPin, endPin);
    }

    reloaded_editor = true;
    std::cout << "Successfully loaded: \""<<filename<<"\"\n";
}

template<typename Fields>
struct FindPinVisitor
{
    ed::PinId id;

    CsgPin* FindPin(std::vector<CsgPin>& inputs, std::vector<CsgPin>& outputs)
    {
        for (CsgPin& pin : inputs)
        {
            if (pin.ID == id)
                return &pin;
        }
        for (CsgPin& pin : outputs)
        {
            if (pin.ID == id)
                return &pin;
        }
        // return null if not found
        return nullptr;
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    CsgPin* operator()(Box<Fields>& expr)
    {
        return FindPin(expr.InputPins, expr.OutputPins);
    }
    CsgPin* operator()(Sphere<Fields>& expr)
    {
        return FindPin(expr.InputPins, expr.OutputPins);
    }
    CsgPin* operator()(Cylinder<Fields>& expr)
    {
        return FindPin(expr.InputPins, expr.OutputPins);
    }
    CsgPin* operator()(PlaneXZ<Fields>& expr)
    {
        return FindPin(expr.InputPins, expr.OutputPins);
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    CsgPin* operator()(Move<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        if (expr.a != nullptr)
            return visit(*this, *expr.a);
        return nullptr;
    }
    CsgPin* operator()(Rotate<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        if (expr.a != nullptr)
            return visit(*this, *expr.a);
        return nullptr;
    }
    CsgPin* operator()(Offset<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        if (expr.a != nullptr)
            return visit(*this, *expr.a);
        return nullptr;
    }
    CsgPin* operator()(Invert<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        if (expr.a != nullptr)
            return visit(*this, *expr.a);
        return nullptr;
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    CsgPin* operator()(Intersect<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        else
        {
            for (auto& leaf : expr.a)
            {
                pin = visit(*this, *leaf);
                if (pin != nullptr)
                    return pin;
            }
            // return null if pin not found on any leaves
            return nullptr;
        }
    }
    CsgPin* operator()(Union<Fields>& expr)
    {
        CsgPin* pin = FindPin(expr.InputPins, expr.OutputPins);
        if (pin != nullptr)
            return pin;
        else
        {
            for (auto& leaf : expr.a)
            {
                pin = visit(*this, *leaf);
                if (pin != nullptr)
                    return pin;
            }
            // return null if pin not found on any leaves
            return nullptr;
        }
    }
};

// Returns the pin object corresponding to the given id
static CsgPin* FindPinCsg(ed::PinId id)
{
    if (!id)
        return nullptr;

    for (auto& root : s_roots)
    {
        // TODO: create visitor obj outside of the loop
        auto result = visit(FindPinVisitor<CsgNode>{id}, *root);
        if (result != nullptr)
            return result;
    }

    return nullptr;
}

// TODO: Unused functions

// Return the link starting from an OutputPin, if one exists
// CsgLink* OutputPinsLink(ed::PinId id)
//{
//    if (!id)
//        return nullptr;
//    for (auto& link : s_Links)
//        // Check only StartPins aka Node->OutputPins
//        if (link.StartPin->ID == id)
//            return &link;
//    return nullptr;
//}

//static bool IsPinLinked(ed::PinId id)
//{
//    if (!id)
//        return false;
//
//    for (auto& link : s_Links)
//        if (link.StartPin->ID == id || link.EndPin->ID == id)
//            return true;
//
//    return false;
//}
//
//static bool CanCreateLink(Pin* a, Pin* b)
//{
//    if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node)
//        return false;
//
//    return true;
//}

static void BuildCsgNode(Expr<CsgNode>* node)
{
    auto inputs = std::visit([](auto& n) {return &n.InputPins; }, *node);
    auto outputs = std::visit([](auto& n) {return &n.OutputPins; }, *node);
    for (auto& input : *inputs)
    {
        input.Node = node;
        input.Kind = PinKind::Input;
    }
    for (auto& output : *outputs)
    {
        output.Node = node;
        output.Kind = PinKind::Output;
    }
}

const char* Application_GetName()
{
    return "CSG Graph Visualizer";
}

void Application_Initialize()
{
    // Reset relevant fields on load
    ResetIdCounter();
    graph_changed = false;

    ed::Config config;

    config.SettingsFile = "CSG_Graph_Visualizer.json";

    config.LoadNodeSettings = [](ed::NodeId nodeId, char* data, void* userPointer) -> size_t
    {
        auto node = FindNode(nodeId);
        if (!node)
            return 0;

        if (data != nullptr)
            memcpy(data, node->State.data(), node->State.size());
        return node->State.size();
    };

    config.SaveNodeSettings = [](ed::NodeId nodeId, const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
    {
        auto node = FindNode(nodeId);
        if (!node)
            return false;

        node->State.assign(data, size);

        TouchNode(nodeId);

        return true;
    };

    m_Editor = ed::CreateEditor(&config);
    ed::SetCurrentEditor(m_Editor);

    //Node* node;
    //node = SpawnInputActionNode();      ed::SetNodePosition(node->ID, ImVec2(-252, 220));
    //node = SpawnBranchNode();           ed::SetNodePosition(node->ID, ImVec2(-300, 351));
    //etc..

    ed::NavigateToContent();

    // TODO: Did i comment this?
    //auto& io = ImGui::GetIO();
}

template<typename Fields>
struct DestructNodesVisitor
{
    void operator()(Box<Fields>& expr)
    {
        delete &expr;
    }
    void operator()(Sphere<Fields>& expr)
    {
        delete &expr;
    }
    void operator()(Cylinder<Fields>& expr)
    {
        delete &expr;
    }
    void operator()(PlaneXZ<Fields>& expr)
    {
        delete &expr;
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    void operator()(Move<Fields>& expr)
    {
        if (expr.a != nullptr)
            visit(*this, *expr.a);
        delete &expr;
    }
    void operator()(Rotate<Fields>& expr)
    {
        if (expr.a != nullptr)
            visit(*this, *expr.a);
        delete &expr;
    }
    void operator()(Offset<Fields>& expr)
    {
        if (expr.a != nullptr)
            visit(*this, *expr.a);
        delete &expr;
    }
    void operator()(Invert<Fields>& expr)
    {
        if (expr.a != nullptr)
            visit(*this, *expr.a);
        delete &expr;
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    void operator()(Intersect<Fields>& expr)
    {
        for (auto& leaf : expr.a)
            visit(*this, *leaf);
        delete &expr;
    }
    void operator()(Union<Fields>& expr)
    {
        for (auto& leaf : expr.a)
            visit(*this, *leaf);
        delete &expr;
    }
};

void Application_Finalize()
{
    if (m_Editor)
    {
        ed::DestroyEditor(m_Editor);
        m_Editor = nullptr;

        delete everything_root;
        everything_root = nullptr;

        // Recursively delete all nodes
        for (auto& root : s_roots)
            std::visit(DestructNodesVisitor<CsgNode>{}, *root);

        s_roots.clear();
        s_Links.clear();
    }
}

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Flow:     return ImColor(255, 255, 255);
    case PinType::Bool:     return ImColor(220, 48, 48);
    case PinType::Int:      return ImColor(68, 201, 156);
    case PinType::Float:    return ImColor(147, 226, 74);
    case PinType::String:   return ImColor(124, 21, 153);
    case PinType::Object:   return ImColor(51, 150, 215);
    case PinType::Function: return ImColor(218, 0, 183);
    case PinType::Delegate: return ImColor(255, 48, 48);
    }
};
    
void ShowStyleEditor(bool* show = nullptr)
{
    if (!ImGui::Begin("Style", show))
    {
        ImGui::End();
        return;
    }

    auto paneWidth = ImGui::GetContentRegionAvailWidth();

    auto& editorStyle = ed::GetStyle();
    //ImGui::BeginHorizontal("Style buttons", ImVec2(paneWidth, 0), 1.0f);
    ImGui::TextUnformatted("Values");
    //ImGui::Spring();
    if (ImGui::Button("Reset to defaults"))
        editorStyle = ed::Style();
    //ImGui::EndHorizontal();
    ImGui::Spacing();
    ImGui::DragFloat4("Node Padding", &editorStyle.NodePadding.x, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Node Rounding", &editorStyle.NodeRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Node Border Width", &editorStyle.NodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Hovered Node Border Width", &editorStyle.HoveredNodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Selected Node Border Width", &editorStyle.SelectedNodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Pin Rounding", &editorStyle.PinRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Pin Border Width", &editorStyle.PinBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Link Strength", &editorStyle.LinkStrength, 1.0f, 0.0f, 500.0f);
    //ImVec2  SourceDirection;
    //ImVec2  TargetDirection;
    ImGui::DragFloat("Scroll Duration", &editorStyle.ScrollDuration, 0.001f, 0.0f, 2.0f);
    ImGui::DragFloat("Flow Marker Distance", &editorStyle.FlowMarkerDistance, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("Flow Speed", &editorStyle.FlowSpeed, 1.0f, 1.0f, 2000.0f);
    ImGui::DragFloat("Flow Duration", &editorStyle.FlowDuration, 0.001f, 0.0f, 5.0f);
    //ImVec2  PivotAlignment;
    //ImVec2  PivotSize;
    //ImVec2  PivotScale;
    //float   PinCorners;
    //float   PinRadius;
    //float   PinArrowSize;
    //float   PinArrowWidth;
    ImGui::DragFloat("Group Rounding", &editorStyle.GroupRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Group Border Width", &editorStyle.GroupBorderWidth, 0.1f, 0.0f, 15.0f);

    ImGui::Separator();

    static ImGuiColorEditFlags edit_mode = ImGuiColorEditFlags_RGB;
    //ImGui::BeginHorizontal("Color Mode", ImVec2(paneWidth, 0), 1.0f);
    ImGui::TextUnformatted("Filter Colors");
    //ImGui::Spring();
    ImGui::RadioButton("RGB", &edit_mode, ImGuiColorEditFlags_RGB);
    //ImGui::Spring(0);
    ImGui::RadioButton("HSV", &edit_mode, ImGuiColorEditFlags_HSV);
    //ImGui::Spring(0);
    ImGui::RadioButton("HEX", &edit_mode, ImGuiColorEditFlags_HEX);
    //ImGui::EndHorizontal();

    static ImGuiTextFilter filter;
    filter.Draw("", paneWidth);

    ImGui::Spacing();

    ImGui::PushItemWidth(-160);
    for (int i = 0; i < ed::StyleColor_Count; ++i)
    {
        auto name = ed::GetStyleColorName((ed::StyleColor)i);
        if (!filter.PassFilter(name))
            continue;

        ImGui::ColorEdit4(name, &editorStyle.Colors[i].x, edit_mode);
    }
    ImGui::PopItemWidth();

    ImGui::End();
}

void ShowLeftPane(float paneWidth)
{
    auto& io = ImGui::GetIO();

    ImGui::BeginChild("Selection", ImVec2(paneWidth, 0));

    paneWidth = ImGui::GetContentRegionAvailWidth();

    static bool showStyleEditor = false;
    //ImGui::BeginHorizontal("Style Editor", ImVec2(paneWidth, 0));
    //ImGui::Spring(0.0f, 0.0f);
    if (ImGui::Button("Zoom to Content"))
        ed::NavigateToContent();
    //ImGui::Spring(0.0f);
    if (ImGui::Button("Show Flow"))
    {
        for (auto& link : s_Links)
            ed::Flow(link.ID);
    }
    //ImGui::Spring();

    // TODO_TEMP uncomment if needed
    //if (ImGui::Button("Edit Style"))
    //    showStyleEditor = true;
    ////ImGui::EndHorizontal();

    //if (showStyleEditor)
    //    ShowStyleEditor(&showStyleEditor);

    std::vector<ed::NodeId> selectedNodes;
    std::vector<ed::LinkId> selectedLinks;
    selectedNodes.resize(ed::GetSelectedObjectCount());
    selectedLinks.resize(ed::GetSelectedObjectCount());

    int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
    int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

    selectedNodes.resize(nodeCount);
    selectedLinks.resize(linkCount);

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetCursorScreenPos(),
        ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
        ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
    ImGui::Spacing(); ImGui::SameLine();
    ImGui::TextUnformatted("Nodes");
    ImGui::Indent();
    for (auto& node : s_Nodes)
    {
        ImGui::PushID(node.ID.AsPointer());
        auto start = ImGui::GetCursorScreenPos();

        if (const auto progress = GetTouchProgress(node.ID))
        {
            ImGui::GetWindowDrawList()->AddLine(
                start + ImVec2(-8, 0),
                start + ImVec2(-8, ImGui::GetTextLineHeight()),
                IM_COL32(255, 0, 0, 255 - (int)(255 * progress)), 4.0f);
        }

        bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node.ID) != selectedNodes.end();
        if (ImGui::Selectable((node.Name + "##" + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer()))).c_str(), &isSelected))
        {
            if (io.KeyCtrl)
            {
                if (isSelected)
                    ed::SelectNode(node.ID, true);
                else
                    ed::DeselectNode(node.ID);
            }
            else
                ed::SelectNode(node.ID, false);

            ed::NavigateToSelection();
        }
        if (ImGui::IsItemHovered() && !node.State.empty())
            ImGui::SetTooltip("State: %s", node.State.c_str());

        //auto id = std::string("(") + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer())) + ")";
        //auto textSize = ImGui::CalcTextSize(id.c_str(), nullptr);
        //auto iconPanelPos = start + ImVec2(
        //    paneWidth - ImGui::GetStyle().FramePadding.x - ImGui::GetStyle().IndentSpacing - saveIconWidth - restoreIconWidth - ImGui::GetStyle().ItemInnerSpacing.x * 1,
        //    (ImGui::GetTextLineHeight() - saveIconHeight) / 2);
        //ImGui::GetWindowDrawList()->AddText(
        //    ImVec2(iconPanelPos.x - textSize.x - ImGui::GetStyle().ItemInnerSpacing.x, start.y),
        //    IM_COL32(255, 255, 255, 255), id.c_str(), nullptr);

        //auto drawList = ImGui::GetWindowDrawList();
        //ImGui::SetCursorScreenPos(iconPanelPos);
        //ImGui::SetItemAllowOverlap();
        //if (node.SavedState.empty())
        //{
        //    if (ImGui::InvisibleButton("save", ImVec2((float)saveIconWidth, (float)saveIconHeight)))
        //        node.SavedState = node.State;

        //    if (ImGui::IsItemActive())
        //        drawList->AddImage(s_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
        //    else if (ImGui::IsItemHovered())
        //        drawList->AddImage(s_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
        //    else
        //        drawList->AddImage(s_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
        //}
        //else
        //{
        //    ImGui::Dummy(ImVec2((float)saveIconWidth, (float)saveIconHeight));
        //    drawList->AddImage(s_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
        //}

        //ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        //ImGui::SetItemAllowOverlap();
        //if (!node.SavedState.empty())
        //{
        //    if (ImGui::InvisibleButton("restore", ImVec2((float)restoreIconWidth, (float)restoreIconHeight)))
        //    {
        //        node.State = node.SavedState;
        //        ed::RestoreNodeState(node.ID);
        //        node.SavedState.clear();
        //    }

        //    if (ImGui::IsItemActive())
        //        drawList->AddImage(s_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
        //    else if (ImGui::IsItemHovered())
        //        drawList->AddImage(s_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
        //    else
        //        drawList->AddImage(s_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
        //}
        //else
        //{
        //    ImGui::Dummy(ImVec2((float)restoreIconWidth, (float)restoreIconHeight));
        //    drawList->AddImage(s_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
        //}

        //ImGui::SameLine(0, 0);
        //ImGui::SetItemAllowOverlap();
        //ImGui::Dummy(ImVec2(0, (float)restoreIconHeight));

        ImGui::PopID();
    }
    ImGui::Unindent();

    static int changeCount = 0;

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetCursorScreenPos(),
        ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
        ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
    ImGui::Spacing(); ImGui::SameLine();
    ImGui::TextUnformatted("Selection");

    //ImGui::BeginHorizontal("Selection Stats", ImVec2(paneWidth, 0));
    ImGui::Text("Changed %d time%s", changeCount, changeCount > 1 ? "s" : "");
    //--------------------------------------------------------------
    //--------------------------------------------------------------

    // TODO: MYCODE
    //if (ImGui::Button("Select root"))
    //    SelectRootNode();
    //ImGui::SameLine();
    if (ImGui::Button("Generate sdf"))
        ClickGenerateSdf();
    int objcount = 100;
    if (ImGui::Button("Genereate test objects"))
        PerfTestGenerateSdf(objcount);
    if (ImGui::Button("QuickSave"))
        SaveNodes();
    if (ImGui::Button("QuickLoad"))
        LoadNodes();
    if (ImGui::Button("Save"))
    {
        std::string save_directory = std::filesystem::current_path().u8string() + "\\local";
        ifd::FileDialog::Instance().Save("SaveJsonDialog", "Saved nodes to .json file",
                                "Json file (*.json){.json}", save_directory);
    }
    if (ifd::FileDialog::Instance().IsDone("SaveJsonDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            std::string filename = ifd::FileDialog::Instance().GetResult().u8string();
            printf("Saving nodes to: [%s]\n", filename.c_str());
            try
            {
                SaveNodes(filename);
            }
            catch (...) {
                std::cout << "Failed to save nodes to: \"" << filename << "\"\n";
            }
        }
        ifd::FileDialog::Instance().Close();
    }
    if (ImGui::Button("Load"))
    {
        std::string load_directory = std::filesystem::current_path().u8string() + "\\local";
        //ifd::FileDialog::Instance().Open("LoadJsonDialog", "Open a texture", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga}, .*");
        ifd::FileDialog::Instance().Open("LoadJsonDialog", "Open nodes from .json file",
                            "Json file (*.json){.json}", false, load_directory);
    }
    if (ifd::FileDialog::Instance().IsDone("LoadJsonDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            std::string filename = ifd::FileDialog::Instance().GetResult().u8string();
            printf("Loading: [%s]\n", filename.c_str());
            try
            {
                LoadNodes(filename);
            }
            catch (...) {
                std::cout << "Failed to load: \"" << filename << "\"\n";
            }
        }
        ifd::FileDialog::Instance().Close();
    }
    //--------------------------------------------------------------
    //--------------------------------------------------------------
    //ImGui::Spring();
    if (ImGui::Button("Deselect All"))
        ed::ClearSelection();
    //ImGui::EndHorizontal();
    ImGui::Indent();
    for (int i = 0; i < nodeCount; ++i) ImGui::Text("Node (%p)", selectedNodes[i].AsPointer());
    for (int i = 0; i < linkCount; ++i) ImGui::Text("Link (%p)", selectedLinks[i].AsPointer());
    ImGui::Unindent();

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
        for (auto& link : s_Links)
            ed::Flow(link.ID);

    if (ed::HasSelectionChanged())
        ++changeCount;

    ImGui::EndChild();
}

static bool MaterialEditor(int& node_mat)
{
    int mat = node_mat;
    bool mat_changed;
    mat_changed = ImGui::InputInt("Material", &mat);
    if (mat_changed)
    {
        if (mat < 0)
            mat = 0;
        if (mat >= material_array_size)
            mat = material_array_size - 1;

        if (node_mat != mat)
        {
            node_mat = mat;
            return true;
        }
    }
    return false;
}

static bool Create_CsgNode_Box(Box<CsgNode>& node)
{
    bool changed = false;

    ed::PinId  node_TransformPinId = node.InputPins[0].ID;
    ImGui::PushItemWidth(150);
    //if (g_FirstFrame)
    //    ed::SetNodePosition(node_Id, node_pos);
    ed::BeginNode(node.id);
    // TODO: Add/Write/Impl node.name
    ImGui::Text("Box");
    ImGui::SameLine();

    const char* format = "%.1f";
    ImGui::PushItemWidth(120);
    ImGui::PushID(node.id.AsPointer());
    float transl_xyz[3] = { node.x, node.y, node.z };
    changed = changed || ImGui::InputFloat3("XYZ", transl_xyz, format);
    node.x = transl_xyz[0]; node.y = transl_xyz[1]; node.z = transl_xyz[2];
    changed = changed || MaterialEditor(node.material);
    ImGui::PopID();
    ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();
    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Sphere(Sphere<CsgNode> &node)
{
    bool changed = false;

    ed::PinId  node_TransformPinId = node.InputPins[0].ID;
    ImGui::PushItemWidth(150);
    //if (g_FirstFrame)
    //    ed::SetNodePosition(node_Id, node_pos);
    ed::BeginNode(node.id);
    // TODO: Add/Write/Impl node.name
    ImGui::Text("Sphere");
    // kell?
    ImGui::SameLine();

    const char* format = "%.1f";
    ImGui::PushItemWidth(70);
    ImGui::PushID(node.id.AsPointer());
    changed = changed || ImGui::InputFloat("Radius", &node.r, 0.0F, 0.0F, format);
    changed = changed || MaterialEditor(node.material);
    ImGui::PopID();
    ImGui::PopItemWidth();


    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();
    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Cylinder(Cylinder<CsgNode>& node)
{
    bool changed = false;

    ed::PinId  node_TransformPinId = node.InputPins[0].ID;
    ImGui::PushItemWidth(150);
    //if (g_FirstFrame)
    //    ed::SetNodePosition(node_Id, node_pos);
    ed::BeginNode(node.id);
    // TODO: Add/Write/Impl node.name
    ImGui::Text("Cylinder");
    ImGui::SameLine();

    const char* format = "%.1f";
    ImGui::PushItemWidth(120);
    ImGui::PushID(node.id.AsPointer());

    //Dir1d dropdown selector (TODO:fix)
    //background set dir1D (TODO:better)
    //checkbox infinty
    //background set y
    int dir_ind = 0;
    switch (node.dir)
    {
    case (Dir1D::X):
        dir_ind = 0;
        break;
    case (Dir1D::Y):
        dir_ind = 1;
        break;
    case (Dir1D::Z):
        dir_ind = 2;
        break;
    }
    changed = changed || ImGui::RadioButton("X", &dir_ind, 0); ImGui::SameLine();
    changed = changed || ImGui::RadioButton("Y", &dir_ind, 1); ImGui::SameLine();
    changed = changed || ImGui::RadioButton("Z", &dir_ind, 2);
    switch (dir_ind)
    {
    case (0):
        node.dir = Dir1D::X;
        break;
    case (1):
        node.dir = Dir1D::Y;
        break;
    case (2):
        node.dir = Dir1D::Z;
        break;
    default:
        node.dir = Dir1D::X;
        break;
    }
    changed = changed || ImGui::InputFloat("Diameter", &node.x, 0.0F, 0.0F, format);

    // TODO: Maybe save/load this too like rotation data, then y could stay = inf when height not yet entered
    bool is_infinite = (node.y == std::numeric_limits<float>::infinity());
    changed = changed || ImGui::Checkbox("Infinite", &is_infinite);
    if (!is_infinite)
    {
        if (node.y == std::numeric_limits<float>::infinity())
            node.y = 0;
        ImGui::SameLine();
        changed = changed || ImGui::InputFloat("Height", &node.y, 0.0F, 0.0F, format);
    }
    else
    {
        node.y = std::numeric_limits<float>::infinity();
    }
    changed = changed || MaterialEditor(node.material);

    ImGui::PopID();
    ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();
    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_PlaneXZ(PlaneXZ<CsgNode>& node)
{
    bool changed = false;

    ed::PinId  node_TransformPinId = node.InputPins[0].ID;
    ImGui::PushItemWidth(150);
    ed::BeginNode(node.id);
    ImGui::Text("PlaneXZ");
    ImGui::SameLine();

    ImGui::PushItemWidth(70);
    ImGui::PushID(node.id.AsPointer());
    changed = changed || MaterialEditor(node.material);
    ImGui::PopID();
    ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();
    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Move(Move<CsgNode>& node)
{
    bool changed = false;

    ImGui::PushItemWidth(150);

    ed::NodeId node_Id = node.id;
    ed::PinId  node_OutputPinId = node.OutputPins[0].ID;
    ed::PinId  node_TransformPinId = node.InputPins[0].ID;

    ed::BeginNode(node_Id);
    ImGui::Text("Translation");

    ImGui::SameLine();
    // TODO: How is this different from my personal pinkind logic?
    ed::BeginPin(node_OutputPinId, ed::PinKind::Output);
    ImGui::Text(">>");
    ed::EndPin();

    const char* format = "%.1f";
    
    // TODO: kell itmewidth?
    //ImGui::PushItemWidth(50);
    ImGui::PushID(node.id.AsPointer());
    float transl_xyz[3] = { node.v.x, node.v.y, node.v.z };
    changed = changed || ImGui::InputFloat3("XYZ", transl_xyz, format);
    //node.v = glm::make_vec3(data);
    node.v.x = transl_xyz[0]; node.v.y = transl_xyz[1]; node.v.z = transl_xyz[2];
    ImGui::PopID();
    //ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();

    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Rotate(Rotate<CsgNode>& node)
{
    bool changed = false;

    ImGui::PushItemWidth(150);

    ed::NodeId node_Id = node.id;
    ed::PinId  node_OutputPinId = node.OutputPins[0].ID;
    ed::PinId  node_TransformPinId = node.InputPins[0].ID;

    ed::BeginNode(node_Id);
    ImGui::Text("Rotation");

    ImGui::SameLine();
    // TODO: How is this different from my personal pinkind logic?
    ed::BeginPin(node_OutputPinId, ed::PinKind::Output);
    ImGui::Text(">>");
    ed::EndPin();

    const char* format = "%.3f";

    // TODO: kell itmewidth?
    //ImGui::PushItemWidth(50);
    ImGui::PushID(node.id.AsPointer());

    float row1[3] = { node.m[0].x, node.m[1].x, node.m[2].x, };
    float row2[3] = { node.m[0].y, node.m[1].y, node.m[2].y, };
    float row3[3] = { node.m[0].z, node.m[1].z, node.m[2].z, };

    //static int rtype_ind = 0;
    //changed = changed || ImGui::Combo("##c", &rtype_ind, "Around X\0Around Y\0Around Z\0By Mat3x3\0\0");
    //changed = changed || ImGui::InputInt("(0-3)Rotation type", &rtype_ind);

    // global static vector, struct: nodeid, angle, rotdir enum
    // TODO: load rtype_ind from globalvector

    int rtype_ind = node.ui_rotation_type;
    changed = changed || ImGui::RadioButton("XYZ", &rtype_ind, 0); ImGui::SameLine();
    changed = changed || ImGui::RadioButton("Around Vec3", &rtype_ind, 1); ImGui::SameLine();
    ImGui::RadioButton("M3x3", &rtype_ind, 2);
    node.ui_rotation_type = static_cast<Rotate<CsgNode>::rotation_type>(rtype_ind);
    // TODO: save rtype_ind to global vector

    // TODO: load angle from globalvector
    if (rtype_ind == 0)
    {
        changed = changed || ImGui::InputInt("Around X", &node.ui_x_degree);
        changed = changed || ImGui::InputInt("Around Y", &node.ui_y_degree);
        changed = changed || ImGui::InputInt("Around Z", &node.ui_z_degree);
        // TODO: save angle to global vector
    }
    if (rtype_ind == 1)
    {
        // TODO: no recompile (MAYBE)
        float axis_vector_xyz[3] = { node.ui_v.x, node.ui_v.y, node.ui_v.z };
        changed = changed || ImGui::InputFloat3("Vector",axis_vector_xyz, "%.1f");
        node.ui_v.x = axis_vector_xyz[0]; node.ui_v.y = axis_vector_xyz[1]; node.ui_v.z = axis_vector_xyz[2];
        changed = changed || ImGui::InputFloat("Angle", &node.ui_vec_radian,1.0F,0.0F, "%.1f");
    }
    if (rtype_ind == 2 /*TODO:Remove if no debuging*/ /* || true*/)
    {
        //ImGui::PushID(&row1);
        changed = changed || ImGui::InputFloat3("##1", row1, format);
        //ImGui::PopID();
        changed = changed || ImGui::InputFloat3("##2", row2, format);
        changed = changed || ImGui::InputFloat3("##3", row3, format);
    }
    if (changed)
    {
        switch (rtype_ind)
        {
        case (0):
            glm::mat3 m_rotx = node.ui_x_degree != 0 ? rotateXdeg(node.ui_x_degree) : glm::mat3(1);
            glm::mat3 m_roty = node.ui_y_degree != 0 ? rotateYdeg(node.ui_y_degree) : glm::mat3(1);
            glm::mat3 m_rotz = node.ui_z_degree != 0 ? rotateZdeg(node.ui_z_degree) : glm::mat3(1);
            node.m = m_rotz * m_roty * m_rotx;
            break;
        case (1):
            if (node.ui_v == glm::vec3(0) || node.ui_vec_radian == 0)
            {
                changed = false;
                break;
            }                
            else
            {
                glm::mat4 m_rotm(1);
                node.m = glm::rotate(m_rotm, (float)glm::radians(node.ui_vec_radian), node.ui_v);
                break;
            }
        case (2):
            // glm uses column-wise assignment
            node.m = glm::mat3(row1[0], row2[0], row3[0], row1[1], row2[1], row3[1], row1[2], row2[2], row3[2]);
            // Incorrect row-wise assignment
            //node.m = mat3(row1[0], row1[1], row1[2],
            //    row2[0], row2[1], row2[2],
            //    row3[0], row3[1], row3[2]);
            break;
        default:
            node.m = glm::mat3(row1[0], row2[0], row3[0], row1[1], row2[1], row3[1], row1[2], row2[2], row3[2]);
            break;
        }
    }
    ImGui::PopID();
    //ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();

    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Offset(Offset<CsgNode>& node)
{
    bool changed = false;

    ImGui::PushItemWidth(150);

    ed::NodeId node_Id = node.id;
    ed::PinId  node_OutputPinId = node.OutputPins[0].ID;
    ed::PinId  node_TransformPinId = node.InputPins[0].ID;

    ed::BeginNode(node_Id);
    ImGui::Text("Offset");

    ImGui::SameLine();
    // TODO: How is this different from my personal pinkind logic?
    ed::BeginPin(node_OutputPinId, ed::PinKind::Output);
    ImGui::Text(">>");
    ed::EndPin();

    const char* format = "%.1f";

    // TODO: kell itmewidth?
    //ImGui::PushItemWidth(50);
    ImGui::PushID(node.id.AsPointer());
    changed = changed || ImGui::InputFloat("Offset", &node.r, 0.0F, 0.0F, format);
    ImGui::PopID();
    //ImGui::PopItemWidth();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();

    ed::EndNode();
    ImGui::PopItemWidth();

    return changed;
}

static bool Create_CsgNode_Invert(Invert<CsgNode>& node)
{
    ImGui::PushItemWidth(150);

    ed::NodeId node_Id = node.id;
    ed::PinId  node_OutputPinId = node.OutputPins[0].ID;
    ed::PinId  node_TransformPinId = node.InputPins[0].ID;

    ed::BeginNode(node_Id);
    ImGui::Text("Invert");

    ImGui::SameLine();
    ed::BeginPin(node_OutputPinId, ed::PinKind::Output);
    ImGui::Text(">>");
    ed::EndPin();

    ed::BeginPin(node_TransformPinId, ed::PinKind::Input);
    ImGui::Text(">>");
    ed::EndPin();

    ed::EndNode();
    ImGui::PopItemWidth();

    return false;
}

static bool Create_CsgNode_Intersect(Intersect<CsgNode>& node)
{
    ed::BeginNode(node.id);
    ImGui::Text("Intersect");

    ed::BeginPin(node.InputPins[0].ID, ed::PinKind::Input);
    ImGui::Text(">");
    ed::EndPin();

    //ImGui::SameLine();
    for (auto& outputpin : node.OutputPins)
    {
        ed::BeginPin(outputpin.ID, ed::PinKind::Output);
        ImGui::Text("   >>");
        ed::EndPin();
    }

    ed::EndNode();
    return false;
}

static bool Create_CsgNode_Union(Union<CsgNode>& node)
{
#if 0
    //const float rounding = 5.0f;
    //const float padding = 12.0f;

    //const auto pinBackground = ed::GetStyle().Colors[ed::StyleColor_NodeBg];

    //ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(128, 128, 128, 200));
    //ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(32, 32, 32, 200));
    //ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(60, 180, 255, 150));
    //ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(60, 180, 255, 150));

    //ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
    //ed::PushStyleVar(ed::StyleVar_NodeRounding, rounding);
    //ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(0.0f, 1.0f));
    //ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
    //ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
    //ed::PushStyleVar(ed::StyleVar_PinBorderWidth, 1.0f);
    //ed::PushStyleVar(ed::StyleVar_PinRadius, 5.0f);
    //ed::BeginNode(node.ID);

    //ImGui::BeginVertical(node.ID.AsPointer());
    //ImGui::BeginHorizontal("inputs");
    //ImGui::Spring(0, padding * 2);

    //ImRect inputsRect;
    //int inputAlpha = 200;
    //if (!node.Inputs.empty())
    //{
    //    auto& pin = node.Inputs[0];
    //    ImGui::Dummy(ImVec2(0, padding));
    //    ImGui::Spring(1, 0);
    //    inputsRect = ImGui_GetItemRect();

    //    ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
    //    ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
    //    ed::PushStyleVar(ed::StyleVar_PinCorners, 12);
    //    ed::BeginPin(pin.ID, ed::PinKind::Input);
    //    ed::PinPivotRect(inputsRect.GetTL(), inputsRect.GetBR());
    //    ed::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
    //    ed::EndPin();
    //    ed::PopStyleVar(3);

    //    if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
    //        inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
    //}
    //else
    //    ImGui::Dummy(ImVec2(0, padding));

    //ImGui::Spring(0, padding * 2);
    //ImGui::EndHorizontal();

    //ImGui::BeginHorizontal("content_frame");
    //ImGui::Spring(1, padding);

    //ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
    //ImGui::Dummy(ImVec2(160, 0));
    //ImGui::Spring(1);
    //ImGui::TextUnformatted(node.Name.c_str());
    //ImGui::Spring(1);
    //ImGui::EndVertical();
    //auto contentRect = ImGui_GetItemRect();

    //ImGui::Spring(1, padding);
    //ImGui::EndHorizontal();

    //ImGui::BeginHorizontal("outputs");
    //ImGui::Spring(0, padding * 2);

    //ImRect outputsRect;
    //int outputAlpha = 200;
    //if (!node.Outputs.empty())
    //{
    //    auto& pin = node.Outputs[0];
    //    ImGui::Dummy(ImVec2(0, padding));
    //    ImGui::Spring(1, 0);
    //    outputsRect = ImGui_GetItemRect();

    //    ed::PushStyleVar(ed::StyleVar_PinCorners, 3);
    //    ed::BeginPin(pin.ID, ed::PinKind::Output);
    //    ed::PinPivotRect(outputsRect.GetTL(), outputsRect.GetBR());
    //    ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
    //    ed::EndPin();
    //    ed::PopStyleVar();

    //    if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
    //        outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
    //}
    //else
    //    ImGui::Dummy(ImVec2(0, padding));

    //ImGui::Spring(0, padding * 2);
    //ImGui::EndHorizontal();

    //ImGui::EndVertical();

    //ed::EndNode();
    //ed::PopStyleVar(7);
    //ed::PopStyleColor(4);

    //auto drawList = ed::GetNodeBackgroundDrawList(node.ID);

    ////const auto fringeScale = ImGui::GetStyle().AntiAliasFringeScale;
    ////const auto unitSize    = 1.0f / fringeScale;

    ////const auto ImDrawList_AddRect = [](ImDrawList* drawList, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int rounding_corners, float thickness)
    ////{
    ////    if ((col >> 24) == 0)
    ////        return;
    ////    drawList->PathRect(a, b, rounding, rounding_corners);
    ////    drawList->PathStroke(col, true, thickness);
    ////};

    //drawList->AddRectFilled(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
    //    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
    ////ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
    //drawList->AddRect(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
    //    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
    ////ImGui::PopStyleVar();
    //drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
    //    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
    ////ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
    //drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
    //    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
    ////ImGui::PopStyleVar();
    //drawList->AddRectFilled(contentRect.GetTL(), contentRect.GetBR(), IM_COL32(24, 64, 128, 200), 0.0f);
    ////ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
    //drawList->AddRect(
    //    contentRect.GetTL(),
    //    contentRect.GetBR(),
    //    IM_COL32(48, 128, 255, 100), 0.0f);
    ////ImGui::PopStyleVar();
#endif
    ed::BeginNode(node.id);
    ImGui::Text("Union");

    ed::BeginPin(node.InputPins[0].ID, ed::PinKind::Input);
    ImGui::Text(">");
    ed::EndPin();

    //ImGui::SameLine();
    for (auto& outputpin : node.OutputPins)
    {
        ed::BeginPin(outputpin.ID, ed::PinKind::Output);
        ImGui::Text(">>");
        ed::EndPin();
    }

    ed::EndNode();
    return false;
}

template<typename Fields>
struct RenderUiVisitor
{
    bool operator()(Box<Fields>& expr)
    {
        return Create_CsgNode_Box(expr);
    }
    bool operator()(Sphere<Fields>& expr)
    {
        return Create_CsgNode_Sphere(expr);
    }
    bool operator()(Cylinder<Fields>& expr)
    {
        return Create_CsgNode_Cylinder(expr);
    }
    bool operator()(PlaneXZ<Fields>& expr)
    {
        return Create_CsgNode_PlaneXZ(expr);
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    bool operator()(Move<Fields>& expr)
    {
        bool changed = Create_CsgNode_Move(expr);        
        if (expr.a != nullptr)
            changed = changed || visit(*this, *expr.a);
        return changed;
    }
    bool operator()(Rotate<Fields>& expr)
    {
        bool changed = Create_CsgNode_Rotate(expr);
        if (expr.a != nullptr)
            changed = changed || visit(*this, *expr.a);
        return changed;
    }
    bool operator()(Offset<Fields>& expr)
    {
        bool changed = Create_CsgNode_Offset(expr);
        if (expr.a != nullptr)
            changed = changed || visit(*this, *expr.a);
        return changed;
    }
    bool operator()(Invert<Fields>& expr)
    {
        bool changed = Create_CsgNode_Invert(expr);
        if (expr.a != nullptr)
            changed = changed || visit(*this, *expr.a);
        return changed;
    }
    // --------------------------------------------------------
    // --------------------------------------------------------
    bool operator()(Intersect<Fields>& expr)
    {
        bool changed = Create_CsgNode_Intersect(expr);
        for (auto& leaf : expr.a)
            changed = changed || visit(*this, *leaf);
        return changed;
    }
    bool operator()(Union<Fields>& expr)
    {
        bool changed = Create_CsgNode_Union(expr);
        for (auto& leaf : expr.a)
            changed = changed || visit(*this, *leaf);
        return changed;
    }
};

template<typename Fields>
struct LinkNodesVisitor
{
    Expr<CsgNode>* child;

    void operator()(Box<Fields>& expr)      { }
    void operator()(Sphere<Fields>& expr)   { }
    void operator()(Cylinder<Fields>& expr) { }
    void operator()(PlaneXZ<Fields>& expr)  { }

    void operator()(Move<Fields>& expr)     { expr.a = child; }
    void operator()(Rotate<Fields>& expr)   { expr.a = child; }
    void operator()(Offset<Fields>& expr)   { expr.a = child; }
    void operator()(Invert<Fields>& expr)   { expr.a = child; }

    void operator()(Intersect<Fields>& expr) { expr.a.emplace_back(child); }
    void operator()(Union<Fields>& expr) { expr.a.emplace_back(child); }
};

template<typename Fields>
struct DeleteLinkVisitor
{
    Expr<CsgNode>* child;

    // TODO raise exception/warning if StartNode is of these types
    void operator()(Box<Fields>& expr) { }
    void operator()(Sphere<Fields>& expr) { }
    void operator()(Cylinder<Fields>& expr) { }
    void operator()(PlaneXZ<Fields>& expr) { }

    void operator()(Move<Fields>& expr) { expr.a = nullptr; }
    void operator()(Rotate<Fields>& expr) { expr.a = nullptr; }
    void operator()(Offset<Fields>& expr) { expr.a = nullptr; }
    void operator()(Invert<Fields>& expr) { expr.a = nullptr; }

    void operator()(Intersect<Fields>& expr)
    {
        // TODO: Does this change the ordering inside the vector?
        // It does remove every occurence
        expr.a.erase(std::remove(expr.a.begin(), expr.a.end(), child), expr.a.end());
    }
    void operator()(Union<Fields>& expr)
    {
        expr.a.erase(std::remove(expr.a.begin(), expr.a.end(), child), expr.a.end());
    }
};

static void ConnectCsgNodes(CsgPin* startPin, CsgPin* endPin)
{
    ed::PinId startPinId = startPin->ID;
    // If there is already an object connected to the startpin/outputpin, disconnect it
    // TODO: refactor into function along with deletelink place one
    auto existing_child_link =
        std::find_if(s_Links.begin(), s_Links.end(),
            [startPinId](auto& link) { return link.StartPin->ID == startPinId; });
    // If another connection found
    if (existing_child_link != s_Links.end())
    {

        std::visit(DeleteLinkVisitor<CsgNode>{ existing_child_link->EndPin->Node },
            * existing_child_link->StartPin->Node);

        // Check if the EndNode can still be found as child in other nodes
        ed::NodeId existing_child_id = std::visit([](auto& n) {return n.id; },
            *existing_child_link->EndPin->Node);
        // Assume it is no longer linked, then try to find if it still is
        bool no_longer_linked = true;
        for (auto& root : s_roots)
        {
            // Try to find the EndNode in other nodes
            if (std::visit(FindNodeVisitor<CsgNode>{existing_child_id, root}, * root))
            {
                // If found, then it is still linked to other node(s)
                no_longer_linked = false;
                break;
            }
        }
        // If the node is no longer linked to anything, add it back as root
        if (no_longer_linked)
            s_roots.emplace_back(existing_child_link->EndPin->Node);

        s_Links.erase(existing_child_link);
    }
    // Create link visually
    s_Links.emplace_back(CsgLink(GetNextId(), startPin, endPin));
    s_Links.back().Color = GetIconColor(startPin->Type);
    // Connect the node expressions
    std::visit(LinkNodesVisitor<CsgNode>{ endPin->Node }, * startPin->Node);
    // Remove the connected child from roots
    s_roots.erase(std::remove(s_roots.begin(), s_roots.end(), endPin->Node), s_roots.end());
}

bool Application_Frame()
{
    UpdateTouch();

    auto& io = ImGui::GetIO();

    ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Roots size: %d", s_roots.size());

    ed::SetCurrentEditor(m_Editor);
    //auto& style = ImGui::GetStyle();

# if 0
    {
        for (auto x = -io.DisplaySize.y; x < io.DisplaySize.x; x += 10.0f)
        {
            ImGui::GetWindowDrawList()->AddLine(ImVec2(x, 0), ImVec2(x + io.DisplaySize.y, io.DisplaySize.y),
                IM_COL32(255, 255, 0, 255));
        }
    }
# endif

    static ed::NodeId contextNodeId = 0;
    static ed::LinkId contextLinkId = 0;
    static ed::PinId  contextPinId = 0;
    static bool createNewNode = false;
    static CsgPin* newNodeLinkPin = nullptr;
    static CsgPin* newLinkPin = nullptr;

    static float leftPaneWidth = 200.0f;
    static float rightPaneWidth = 800.0f;
    Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

    // Set true by GenerateSdf() button in ShowLeftPane if pressed and successfully generated
    generate_clicked = false;
    graph_changed = false;
    // GetMousePos() value depends on context, always set this at the relevant location
    ImVec2 openPopupPosition;

    ShowLeftPane(leftPaneWidth - 4.0f);    
    ImGui::SameLine(0.0f, 12.0f);

    static int navigate_framedelay = 0;
    // Navigate to nodes loaded from .json file (by clicking Load button on UI)
    if (reloaded_editor)
    {
        // Need a few frames delay after loading nodes for ed::NavigateToContent() to work
        if (navigate_framedelay++ >= 2)
        {
            ed::NavigateToContent();
            reloaded_editor = false;
            navigate_framedelay = 0;
        }
    }

    ed::Begin("Node editor");
    {
        auto cursorTopLeft = ImGui::GetCursorScreenPos();
        // Display existing nodes
        for (auto& root : s_roots)
            graph_changed = graph_changed || visit(RenderUiVisitor<CsgNode>{}, *root);

        // Display existing links
        for (auto& link : s_Links)
            ed::Link(link.ID, link.StartPin->ID, link.EndPin->ID, link.Color, 2.0f);

        // Create/delete links
        if (!createNewNode)
        {
            if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
            {
                auto showLabel = [](const char* label, ImColor color)
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
                    auto size = ImGui::CalcTextSize(label);

                    auto padding = ImGui::GetStyle().FramePadding;
                    auto spacing = ImGui::GetStyle().ItemSpacing;

                    ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

                    auto rectMin = ImGui::GetCursorScreenPos() - padding;
                    auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

                    auto drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
                    ImGui::TextUnformatted(label);
                };

                ed::PinId startPinId = 0, endPinId = 0;
                if (ed::QueryNewLink(&startPinId, &endPinId))
                {                    
                    auto startPin = FindPinCsg(startPinId);
                    auto endPin = FindPinCsg(endPinId);

                    newLinkPin = startPin ? startPin : endPin;

                    // Connecting line can be drawn both ways, store them correctly
                    if (startPin->Kind == PinKind::Input)
                    {
                        std::swap(startPin, endPin);
                        std::swap(startPinId, endPinId);
                    }

                    if (startPin && endPin)
                    {

                        if (endPin == startPin)
                        {
                            ed::RejectNewItem(ImColor(128, 255, 128), 2.0f);
                        }
                        else if (endPin->Kind == startPin->Kind)
                        {
                            showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
                            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                        }
                        else if (endPin->Node == startPin->Node)
                        {
                            showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
                            ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
                        }
                        else if (endPin->Type != startPin->Type)
                        {
                            showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
                            ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                        }
                        else
                        {
                            // Check if the two nodes are already directly connected
                            // TODO: Is this too slow? Do only when accepted if it is with lots of links
                            auto already_connected_link =
                                std::find_if(s_Links.begin(), s_Links.end(),
                                    [endPinId, startPin](auto& link) { return link.EndPin->ID == endPinId &&
                                    link.StartPin->Node == startPin->Node; });
                            if (already_connected_link != s_Links.end())
                            {
                                showLabel("x Already connected", ImColor(45, 32, 32, 180));
                                ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                                //std::cout << "asd";
                            }
                            else
                            {
                                // Check if the connection would make a circle
                                ed::NodeId startPin_NodeId = std::visit([](auto& n) {return n.id; }, *startPin->Node);
                                auto parent_found_in_child =  std::visit(FindNodeVisitor<CsgNode>{ startPin_NodeId, endPin->Node }, *endPin->Node);
                                if (parent_found_in_child)
                                {
                                    showLabel("x Cyclic connections are not supported", ImColor(45, 32, 32, 180));
                                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                                    //std::cout << "asd";
                                }
                                else
                                {
                                    showLabel("+ Create Link", ImColor(32, 45, 32, 180));
                                    if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
                                    {
                                        ConnectCsgNodes(startPin, endPin);
                                        graph_changed = true;
                                    }
                                }                                
                            }
                        }
                    }
                }

                openPopupPosition = ImGui::GetMousePos();
                ed::PinId pinId = 0;
                if (ed::QueryNewNode(&pinId))
                {
                    newLinkPin = FindPinCsg(pinId);
                    if (newLinkPin)
                        showLabel("+ Create Node", ImColor(32, 45, 32, 180));

                    if (ed::AcceptNewItem())
                    {
                        createNewNode = true;
                        newNodeLinkPin = FindPinCsg(pinId);
                        newLinkPin = nullptr;
                        ed::Suspend();
                        ImGui::OpenPopup("Create New Node");
                        newNodePosition = openPopupPosition;
                        ed::Resume();
                    }
                }
            }
            else
                newLinkPin = nullptr;
            ed::EndCreate();

            if (ed::BeginDelete())
            {
                ed::LinkId linkId = 0;
                while (ed::QueryDeletedLink(&linkId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        // Finds link
                        auto iter_link = std::find_if(s_Links.begin(), s_Links.end(), [linkId](auto& link) { return link.ID == linkId; });
                        if (iter_link != s_Links.end())
                        {
                            // Delete expression/tree linkage
                            std::visit(DeleteLinkVisitor<CsgNode>{ iter_link->EndPin->Node }, *(iter_link->StartPin->Node));
                            
                            // Check if the EndNode can still be found as child in other nodes
                            ed::NodeId end_id = std::visit([](auto& n) {return n.id; }, *iter_link->EndPin->Node);
                            // Assume it is no longer linked, then try to find if it still is
                            bool no_longer_linked = true;
                            for (auto& root : s_roots)
                            {
                                // Try to find the EndNode in other nodes
                                if (std::visit(FindNodeVisitor<CsgNode>{end_id, root}, * root))
                                {
                                    // If found, then it is still linked to other node(s)
                                    no_longer_linked = false;
                                    break;
                                }
                            }
                            // If the node is no longer linked to anything, add it back as root
                            if (no_longer_linked)
                                s_roots.emplace_back(iter_link->EndPin->Node);
                            
                            // Visually delete link in UI
                            s_Links.erase(iter_link);
                            graph_changed = true;
                        }
                    }
                }

                ed::NodeId nodeId = 0;
                while (ed::QueryDeletedNode(&nodeId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        Expr<CsgNode>* node;
                        for (auto& root : s_roots)
                        {
                            node = std::visit(FindNodeVisitor<CsgNode>{nodeId, root}, *root);

                            if (node)
                            {
                                // All links of the node are deleted automatically, so this code is not needed
                                // 
                                //// finds only one output link
                                //auto output_link =
                                //    std::find_if(s_Links.begin(), s_Links.end(),
                                //        //[nodeId](auto& link) { return link.StartPin->Node.id == nodeId; });
                                //        [node](auto& link) { return link.StartPin->Node == node; });

                                //if (output_link != s_Links.end())
                                //{
                                //    std::visit(DeleteLinkVisitor<CsgNode>{ output_link->EndPin->Node },
                                //        *(output_link->StartPin->Node));
                                //    s_roots.emplace_back(output_link->EndPin->Node);
                                //    s_Links.erase(output_link);
                                //}

                                // Deleteing all its links has added it as root
                                s_roots.erase(std::remove(s_roots.begin(), s_roots.end(), node), s_roots.end());
                                delete node;
                                graph_changed = true;
                                break;
                            }
                        }
                    }
                }
            }
            ed::EndDelete();
        }
        ImGui::SetCursorScreenPos(cursorTopLeft);
    }

# if 1
    // Minimized ugly hack, screensize = 4 might be different for other resolution, just set it higher
    bool is_collapsed = (ed::GetScreenSize().y <= 40);
    openPopupPosition = ImGui::GetMousePos();
    // Context menu events (right-cliks)
    if (!is_collapsed)
    {
        ed::Suspend();
        if (ed::ShowNodeContextMenu(&contextNodeId))
            ImGui::OpenPopup("Node Context Menu");
        else if (ed::ShowPinContextMenu(&contextPinId))
            ImGui::OpenPopup("Pin Context Menu");
        else if (ed::ShowLinkContextMenu(&contextLinkId))
            ImGui::OpenPopup("Link Context Menu");
        else if (ed::ShowBackgroundContextMenu())
        {
            ImGui::OpenPopup("Create New Node");
            newNodePosition = openPopupPosition;
            newNodeLinkPin = nullptr;
        }
        ed::Resume();
    }
    
    // Handle context menu events (right-clicks)
    if (!is_collapsed)
    {
        ed::Suspend();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        if (ImGui::BeginPopup("Node Context Menu"))
        {
            auto node = FindNode(contextNodeId);

            ImGui::TextUnformatted("Node Context Menu");
            ImGui::Separator();
            if (node)
            {
                ImGui::Text("ID: %p", node->ID.AsPointer());
                ImGui::Text("Type: %s", node->Type == NodeType::Blueprint ? "Blueprint" : (node->Type == NodeType::Tree ? "Tree" : "Comment"));
                ImGui::Text("Inputs: %d", (int)node->Inputs.size());
                ImGui::Text("Outputs: %d", (int)node->Outputs.size());
            }
            else
                ImGui::Text("Unknown node: %p", contextNodeId.AsPointer());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete"))
                ed::DeleteNode(contextNodeId);
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Pin Context Menu"))
        {
            auto pin = FindPin(contextPinId);

            ImGui::TextUnformatted("Pin Context Menu");
            ImGui::Separator();
            if (pin)
            {
                ImGui::Text("ID: %p", pin->ID.AsPointer());
                if (pin->Node)
                    ImGui::Text("Node: %p", pin->Node->ID.AsPointer());
                else
                    ImGui::Text("Node: %s", "<none>");
            }
            else
                ImGui::Text("Unknown pin: %p", contextPinId.AsPointer());

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Link Context Menu"))
        {
            auto link = FindLink(contextLinkId);

            ImGui::TextUnformatted("Link Context Menu");
            ImGui::Separator();
            if (link)
            {
                ImGui::Text("ID: %p", link->ID.AsPointer());
                ImGui::Text("From: %p", link->StartPin->ID.AsPointer());
                ImGui::Text("To: %p", link->EndPin->ID.AsPointer());
            }
            else
                ImGui::Text("Unknown link: %p", contextLinkId.AsPointer());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete"))
                ed::DeleteLink(contextLinkId);
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Create New Node"))
        {
            Expr<CsgNode>* node = nullptr;

            // Don't show primitives in menu if linking to another node's inputs
            if (newNodeLinkPin && newNodeLinkPin->Kind == PinKind::Input)
            { } // skip
            else
            {
                // Primitives
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0, 255, 0, 255)));
                ImGui::Text("Primitives:");
                ImGui::PopStyleColor();
                ImGui::Separator();
                if (ImGui::MenuItem("Sphere"))
                {
                    node = sphere(0.f, CsgNode{ GetNextId() });
                    s_roots.emplace_back(node);
                    std::get<Sphere<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                }
                if (ImGui::MenuItem("Box"))
                {
                    node = box(0.f, 0.f, 0.f, CsgNode{ GetNextId() });
                    s_roots.emplace_back(node);
                    std::get<Box<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                }
                if (ImGui::MenuItem("Cylinder"))
                {
                    node = cylinder(Dir1D::Y, 0.f, 0.f, CsgNode{ GetNextId() });
                    s_roots.emplace_back(node);
                    std::get<Cylinder<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                }
                if (ImGui::MenuItem("Plane"))
                {
                    node = planeXZ(CsgNode{ GetNextId() });
                    s_roots.emplace_back(node);
                    std::get<PlaneXZ<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                }
                ImGui::Separator();
            }
            // Transformations
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0, 255, 0, 255)));
            ImGui::Text("Transforms:");
            ImGui::PopStyleColor();
            ImGui::Separator();
            if (ImGui::MenuItem("Translation"))
            {
                Expr<CsgNode>* empty = nullptr;
                node = move({ 0.f, 0.f, 0.f }, empty, CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Move<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                std::get<Move<CsgNode>>(*(s_roots.back())).OutputPins.emplace_back(GetNextId(), "<<");
            }
            if (ImGui::MenuItem("Rotation"))
            {
                Expr<CsgNode>* empty = nullptr;
                node = rotate(glm::mat3(1.0), empty, CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Rotate<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                std::get<Rotate<CsgNode>>(*(s_roots.back())).OutputPins.emplace_back(GetNextId(), "<<");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Offset"))
            {
                Expr<CsgNode>* empty = nullptr;
                node = offset(0.f, empty, CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Offset<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                std::get<Offset<CsgNode>>(*(s_roots.back())).OutputPins.emplace_back(GetNextId(), "<<");
            }
            if (ImGui::MenuItem("Invert"))
            {
                Expr<CsgNode>* empty = nullptr;
                node = invert(empty, CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Invert<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                std::get<Invert<CsgNode>>(*(s_roots.back())).OutputPins.emplace_back(GetNextId(), "<<");
            }
            ImGui::Separator();
            // Operations
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0, 255, 0, 255)));
            ImGui::Text("Operations:");
            ImGui::PopStyleColor();
            ImGui::Separator();
            if (ImGui::MenuItem("Union"))
            {
                node = union_op( CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Union<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                auto outputs = &std::get<Union<CsgNode>>(*(s_roots.back())).OutputPins;
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
            }
            if (ImGui::MenuItem("Intersect"))
            {
                node = intersect(CsgNode{ GetNextId() });
                s_roots.emplace_back(node);
                std::get<Intersect<CsgNode>>(*(s_roots.back())).InputPins.emplace_back(GetNextId(), "<<");
                auto outputs = &std::get<Intersect<CsgNode>>(*(s_roots.back())).OutputPins;
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
                outputs->emplace_back(GetNextId(), "<<");
            }

            if (node)
            {
                graph_changed = true;
                createNewNode = false;

                BuildCsgNode(node);

                ed::NodeId new_node_id = std::visit([](auto& n) {return n.id; }, *node);
                ed::SetNodePosition(new_node_id, newNodePosition);

                // If created by dragging link from a pin, newNodeLinkPin won't be null
                if (auto startPin = newNodeLinkPin)
                {
                    std::vector<CsgPin>* pins;
                    
                    if (startPin->Kind == PinKind::Input)
                        pins = std::visit([](auto& n) { return &n.OutputPins; }, *node);
                    else
                        pins = std::visit([](auto& n) { return &n.InputPins; }, *node);

                    if (!pins->empty())
                    {
                        // Connect to the first pin
                        auto endPin = &(*pins)[0];
                        if (startPin->Kind == PinKind::Input)
                            std::swap(startPin, endPin);

                        ConnectCsgNodes(startPin, endPin);
                        graph_changed = true;
                    }
                }
            }

            ImGui::EndPopup();
        }
        else
            createNewNode = false;
        ImGui::PopStyleVar();
        ed::Resume();
    }
# endif

    /*
        cubic_bezier_t c;
        c.p0 = pointf(100, 600);
        c.p1 = pointf(300, 1200);
        c.p2 = pointf(500, 100);
        c.p3 = pointf(900, 600);

        auto drawList = ImGui::GetWindowDrawList();
        auto offset_radius = 15.0f;
        auto acceptPoint = [drawList, offset_radius](const bezier_subdivide_result_t& r)
        {
            drawList->AddCircle(to_imvec(r.point), 4.0f, IM_COL32(255, 0, 255, 255));

            auto nt = r.tangent.normalized();
            nt = pointf(-nt.y, nt.x);

            drawList->AddLine(to_imvec(r.point), to_imvec(r.point + nt * offset_radius), IM_COL32(255, 0, 0, 255), 1.0f);
        };

        drawList->AddBezierCurve(to_imvec(c.p0), to_imvec(c.p1), to_imvec(c.p2), to_imvec(c.p3), IM_COL32(255, 255, 255, 255), 1.0f);
        cubic_bezier_subdivide(acceptPoint, c);
    */

    // Generate new .glsl fragment shader if necessary
    if (graph_changed && render_all)
        AutoGenerateSdf();

    ed::End();
    //ImGui::ShowTestWindow();
    //ImGui::ShowMetricsWindow();
    //ImGui::ShowDemoWindow();
    return (graph_changed && (render_all || generate_clicked));
}

//------------------------------------------------------------------------------
//} // namespace CsgTreeEditor end
