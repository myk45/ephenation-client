#include "UserObject.h"

std::vector<UserObject*> gUserObjects;

UserObject::UserObject()
{

}

UserObject::~UserObject()
{

}

void UserObject::mAnimateObject()
{

}

bool UserObject::mAttachObject(UserObject *other)
{
    return expandObject(other);
}


// @todo: check if object types match!
// expands one object, deletes the other one. So, there is only one entity now.
bool UserObject::expandObject(UserObject *other)
{
    std::vector<UserBlock*> otherBlockList = other->mGetUserBlockList();

    for (unsigned int i = 0; i < otherBlockList.size(); i++) {
        this->mUserBlockList.push_back(otherBlockList[i]);
    }

    other->cleanUpObject();
    return true;
}

bool UserObject::cleanUpObject()
{
    mUserBlockList.clear();

    // Mukund: Check this!
    delete this;
    return true;
}

bool UserObject::mDetachObject(UserObject *other)
{
    return true;
}

void UserObject::mGetLocation() const
{

}


/*!
* @brief Used to create a new object at location coordLoc
*/
UserObject* UserObject::msMakeUserObject(CoordLocation coordLoc, ChunkBlock fChunkBlock)
{
    UserObject* uObj = new UserObject();
    UserBlock* block = new UserBlock;
    block->fObjectType = -1;
    block->fBlockCoord = coordLoc;

    // Mukund
    // @todo May not be necessary after all!
    block->fChunkBlock.x = fChunkBlock.x;
    block->fChunkBlock.y = fChunkBlock.y;
    block->fChunkBlock.z = fChunkBlock.z;

    uObj->mUserBlockList.push_back(block);
    return uObj;
}


