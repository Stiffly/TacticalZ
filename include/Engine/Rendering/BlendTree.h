#ifndef BlendTree_h__
#define BlendTree_h__

#include "Common.h"
#include "../GLM.h"
#include "Skeleton.h"
#include "../Core/EntityWrapper.h"
#include "../Core/World.h"
#include <stack>

class BlendTree
{
public:
    enum class NodeType
    {
        Additive,
        Blend,
        Override,
        Animation,
    };

    

    struct Node
    {
        std::string Name;
        EntityWrapper Entity;
        Node* Parent = nullptr;
        Node* Child[2] = { nullptr, nullptr };
        NodeType Type;
        std::map<int, Skeleton::PoseData> Pose;
        bool SubTreeRoot = false;
        double Weight = 0.0;

        Node* Next() {
            Node* next = this;

            if (next->Child[1] == nullptr) {
                // Node has no right child
                next = this;
                while (next->Parent != nullptr && next == next->Parent->Child[1]) {
                    next = next->Parent;
                }
                next = next->Parent;
            } else {
                // Find the leftmost node in the right subtree
                next = next->Child[1];
                while (next->Child[0] != nullptr) {
                    next = next->Child[0];
                }
            }
            return next;
        }
    };

  
    struct AutoBlendInfo
    {
        std::string NodeName;
        double progress;
        bool Start;
        bool SingleBlend;
        double Weight;
        std::unordered_map<EntityWrapper, double> StartWeights;
    };

    

    BlendTree(EntityWrapper ModelEntity, Skeleton* skeleton);
    ~BlendTree();


    std::vector<glm::mat4> GetFinalPose() { return m_FinalPose; }
    glm::mat4 GetBoneTransform(int boneID);
    bool IsValid() { return  (m_Root == nullptr ? false : true); }
    void PrintTree();
    BlendTree::AutoBlendInfo AutoBlendStep(AutoBlendInfo blendInfo);

    BlendTree::Node* GetCommonParent(std::string NodeName1, std::string NodeName2);
    BlendTree::Node* FirstCommonParent(Node* node1, Node* node2);

    EntityWrapper GetSubTreeRoot(std::string nodeName);
    std::vector<EntityWrapper> GetSubTreeRoots(std::string nodeName);

    std::vector<EntityWrapper> GetSingleLevelRoots(std::string name);
    std::vector<EntityWrapper> GetEntitesByName(std::string name);

    void SetWeightByName(std::string name, double weight);

private:
    Skeleton* m_Skeleton = nullptr;
    Node* m_Root = nullptr;

    std::vector<glm::mat4> m_FinalPose;
    std::map<int, glm::mat4> m_FinalBoneTransforms;
    std::vector<BlendTree::Node*> FindNodesByName(std::string name);
    std::vector<glm::mat4> AccumulateFinalPose();
    BlendTree::Node* FillTreeByName(Node* parentNode, std::string name, EntityWrapper parentEntity);

    void Blend(std::map<int, Skeleton::PoseData>& pose);
};

#endif
