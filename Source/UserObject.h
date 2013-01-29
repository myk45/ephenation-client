#ifndef _USER_OBJECT_
#define _USER_OBJECT_


#include "glm/glm.hpp"
#include <vector>

typedef glm::vec3 CoordLocation;
typedef glm::vec3 ObjectLocation;
typedef glm::vec3 ChunkBlock;

/*!
* @brief A list of the below blocks composes the user defined object
* @todo  Use macros in gamedialog.cpp instead of numbers. would be more readable
*        Just put the "todo" here.
*/
struct UserBlock {
    unsigned char fObjectType;
    CoordLocation   fBlockCoord;
    ChunkBlock      fChunkBlock; // to which chunk the block belongs to. Is this necessary?!
};



/*!
* @brief Different type of user defined objects can be present. All need not have the same
*       behavior.
*/
enum UserOjectType {
    OBJECT_INVALID = -1,
    OBJECT_STATIC  =  0,
    OBJECT_DYNAMIC,
    OBJECT_COUNT
};

/*!
* @brief The user defined object. This is just a list of UserBlocks.
*       This class can be further extended for different types of behavior by different
*       kind of UDOs(User Defined Object)
*
* @todo add to proper M/V/C namespace.
*/
class UserObject {
    protected:
        virtual ~UserObject(); // Let us not allow local vars and not use delete on it.
        std::vector<UserBlock*> mUserBlockList;
        UserOjectType mObjectType;
        ObjectLocation mObjLocation; // Still needs to be seen what this means.
        UserObject();

    public:
        // @todo deprecate this and make a factory?
        static UserObject* msMakeUserObject(CoordLocation, glm::vec3);

        bool mAttachObject(UserObject *other);
        bool mDetachObject(UserObject *other);
        bool cleanUpObject(); // replacing destructor.

        std::vector<UserBlock*> mGetUserBlockList() { return mUserBlockList; }

        void mGetLocation() const; // gets the object's location
        virtual void mAnimateObject(); // This can be overridden to have different behavior of objects.

    protected:
        bool expandObject(UserObject *other);

};

// @todo Just for now! Needs to be in some meaningful place. Not as a global!
extern std::vector<UserObject*> gUserObjects;

#endif
