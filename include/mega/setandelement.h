/**
* @file mega/setandelement.h
* @brief Class for manipulating Sets and their Elements
*
* (c) 2013-2022 by Mega Limited, Wellsford, New Zealand
*
* This file is part of the MEGA SDK - Client Access Engine.
*
* Applications using the MEGA API must present a valid application key
* and comply with the the rules set forth in the Terms of Service.
*
* The MEGA SDK is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* @copyright Simplified (2-clause) BSD License.
*
* You should have received a copy of the license along with this
* program.
*/

#ifndef MEGA_SET_AND_ELEMENT_H
#define MEGA_SET_AND_ELEMENT_H 1

#include "types.h"

#include <bitset>
#include <cassert>
#include <functional>
#include <memory>
#include <string>

namespace mega {

    /**
     * @brief Base class for common characteristics of Set and Element
     */
    class CommonSE
    {
    public:
        // get own id
        const handle& id() const { return mId; }

        // get key used for encrypting attrs
        const std::string& key() const { return mKey; }

        // get timestamp
        const m_time_t& ts() const { return mTs; }

        // get creation timestamp
        const m_time_t& cts() const { return mCTs; }

        // get own name
        const std::string& name() const { return getAttr(nameTag); }

        // set own id
        void setId(handle id) { mId = id; }

        // set key used for encrypting attrs
        void setKey(const std::string& key) { mKey = key; }
        void setKey(std::string&& key) { mKey = move(key); }

        // set timestamp
        void setTs(m_time_t ts) { mTs = ts; }

        // set creation timestamp
        void setCTs(m_time_t ts) { mCTs = ts; }

        // set own name
        void setName(std::string&& name);

        // test for attrs (including empty "" ones)
        bool hasAttrs() const { return !!mAttrs; }

        // test for encrypted attrs, that will need a call to decryptAttributes()
        bool hasEncrAttrs() const { return !!mEncryptedAttrs; }

        // set encrypted attrs, that will need a call to decryptAttributes()
        void setEncryptedAttrs(std::string&& eattrs) { mEncryptedAttrs.reset(new std::string(move(eattrs))); }

        // decrypt attributes set with setEncryptedAttrs(), and replace internal attrs
        bool decryptAttributes(std::function<bool(const std::string&, const std::string&, string_map&)> f);

        // encrypt internal attrs and return the result
        std::string encryptAttributes(std::function<std::string(const string_map&, const std::string&)> f) const;

        static const int HANDLESIZE = 8;

    protected:
        CommonSE() = default;
        CommonSE(handle id, std::string&& key, string_map&& attrs) : mId(id), mKey(move(key)), mAttrs(new string_map(move(attrs))) {}

        handle mId = UNDEF;
        std::string mKey;
        std::unique_ptr<string_map> mAttrs;
        m_time_t mTs = 0;  // timestamp
        m_time_t mCTs = 0; // creation timestamp

        void setAttr(const std::string& tag, std::string&& value); // set any non-standard attr
        const std::string& getAttr(const std::string& tag) const;
        bool hasAttrChanged(const std::string& tag, const std::unique_ptr<string_map>& otherAttrs) const;
        void rebaseCommonAttrsOn(const string_map* baseAttrs);

        static bool validChangeType(const unsigned& typ, const unsigned& typMax) { assert(typ < typMax); return typ < typMax; }

        std::unique_ptr<std::string> mEncryptedAttrs;             // "at": up to 65535 bytes of miscellaneous data, encrypted with mKey

        static const std::string nameTag; // "n", used for 'name' attribute
    };

    /**
     * @brief Internal representation of an Element
     */
    class SetElement : public CommonSE, public Cacheable
    {
    public:
        SetElement() = default;
        SetElement(handle sid, handle node, handle elemId, std::string&& key, string_map&& attrs)
            : CommonSE(elemId, move(key), move(attrs)), mSetId(sid), mNodeHandle(node) {}

        // return id of the set that owns this Element
        const handle& set() const { return mSetId; }

        // return handle of the node represented by this Element
        const handle& node() const { return mNodeHandle; }

        // return order of this Element
        int64_t order() const { return mOrder ? *mOrder : 0; }

        // set id of the set that owns this Element
        void setSet(handle s) { mSetId = s; }

        // set handle of the node represented by this Element
        void setNode(handle nh) { mNodeHandle = nh; }

        // set order of this Element
        void setOrder(int64_t order);

        // return true if last change modified order of this Element
        // (useful for instances that only contain updates)
        bool hasOrder() const { return !!mOrder; }

        // replace internal parameters with the ones of 'el', and mark any CH_EL_XXX change
        bool updateWith(SetElement&& el);

        // apply attrs on top of the ones of 'el' (useful for instances that only contain updates)
        void rebaseAttrsOn(const SetElement& el) { rebaseCommonAttrsOn(el.mAttrs.get()); }

        // mark attrs being cleared by the last update (reason for internal container being null)
        // (useful for instances that only contain updates)
        void setAttrsClearedByLastUpdate(bool cleared) { mAttrsClearedByLastUpdate = cleared; }

        // return true if attrs have been cleared in the last update (reason for internal container being null)
        // (useful for instances that only contain updates)
        bool hasAttrsClearedByLastUpdate() const { return mAttrsClearedByLastUpdate; }

        // mark a change to internal parameters (useful for app notifications)
        void setChanged(int changeType) { if (validChangeType(changeType, CH_EL_SIZE)) mChanges[changeType] = 1; }

        // reset changes of internal parameters (call after app has been notified)
        void resetChanges() { mChanges = 0; }

        // return changes to internal parameters (useful for app notifications)
        unsigned long changes() const { return mChanges.to_ulong(); }

        // return true if internal parameter pointed out by changeType has changed (useful for app notifications)
        bool hasChanged(int changeType) const { return validChangeType(changeType, CH_EL_SIZE) ? mChanges[changeType] : false; }

        bool serialize(std::string*) override;
        static std::unique_ptr<SetElement> unserialize(std::string* d);

        enum // match MegaSetElement::CHANGE_TYPE_ELEM_XXX values
        {
            CH_EL_NEW,      // point out that this is a new Element
            CH_EL_NAME,     // point out that 'name' attr has changed
            CH_EL_ORDER,    // point out that order has changed
            CH_EL_REMOVED,  // point out that this Element has been removed

            CH_EL_SIZE
        };

    private:
        handle mSetId = UNDEF;
        handle mNodeHandle = UNDEF;
        std::unique_ptr<int64_t> mOrder;
        bool mAttrsClearedByLastUpdate = false;

        std::bitset<CH_EL_SIZE> mChanges;
    };

    /**
     * @brief Internal representation of a Set
     */
    class Set : public CommonSE, public Cacheable
    {
    public:
        Set() = default;
        Set(handle id, std::string&& key, handle user, string_map&& attrs)
            : CommonSE(id, move(key), move(attrs)), mUser(user) {}

        // return id of the user that owns this Set
        const handle& user() const { return mUser; }

        // return id of the Element that was set as cover, or UNDEF if none was set
        handle cover() const;

        // set id of the user that owns this Set
        void setUser(handle uh) { mUser = uh; }

        // set id of the Element that will act as cover; pass UNDEF to remove cover
        void setCover(handle h);

        // replace internal parameters with the ones of 's', and mark any CH_XXX change
        bool updateWith(Set&& s);

        // apply attrs on top of the ones of 's' (useful for instances that only contain updates)
        void rebaseAttrsOn(const Set& s) { rebaseCommonAttrsOn(s.mAttrs.get()); }

        // mark a change to internal parameters (useful for app notifications)
        void setChanged(int changeType) { if (validChangeType(changeType, CH_SIZE)) mChanges[changeType] = 1; }

        // reset changes of internal parameters (call after app has been notified)
        void resetChanges() { mChanges = 0; }

        // return changes to internal parameters (useful for app notifications)
        unsigned long changes() const { return mChanges.to_ulong(); }

        // return true if internal parameter pointed out by changeType has changed (useful for app notifications)
        bool hasChanged(int changeType) const { return validChangeType(changeType, CH_SIZE) ? mChanges[changeType] : false; }

        bool serialize(std::string*) override;
        static std::unique_ptr<Set> unserialize(std::string* d);

        enum // match MegaSet::CHANGE_TYPE_XXX values
        {
            CH_NEW,     // point out that this is a new Set
            CH_NAME,    // point out that 'name' attr has changed
            CH_COVER,   // point out that 'cover' attr has changed
            CH_REMOVED, // point out that this Set has been removed

            CH_SIZE
        };

    private:
        handle mUser = UNDEF;

        std::bitset<CH_SIZE> mChanges;

        static const std::string coverTag; // "c", used for 'cover' attribute
    };

} //namespace

#endif

