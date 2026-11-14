// ##########################################################################
// #                                                                        #
// #                              CLOUDCOMPARE                              #
// #                                                                        #
// #  This program is free software; you can redistribute it and/or modify  #
// #  it under the terms of the GNU General Public License as published by  #
// #  the Free Software Foundation; version 2 or later of the License.      #
// #                                                                        #
// #  This program is distributed in the hope that it will be useful,       #
// #  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
// #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
// #  GNU General Public License for more details.                          #
// #                                                                        #
// ##########################################################################

#include "ccSleekUndo.h"

#include "ccDBRoot.h"
#include "mainwindow.h"

// qCC_db
#include <ccGenericMesh.h>
#include <ccLog.h>
#include <ccPointCloud.h>
#include <ccPolyline.h>

namespace
{
	//! An entity that is either in the DB tree (ptr is null) or detached and owned (ptr is set)
	struct Holder
	{
		unsigned   id       = 0;
		unsigned   parentId = 0; // 0 = DB root
		ccHObject* ptr      = nullptr;
	};

	void CollectIDs(const ccHObject* obj, std::set<unsigned>& ids)
	{
		for (unsigned i = 0; i < obj->getChildrenNumber(); ++i)
		{
			const ccHObject* child = obj->getChild(i);
			ids.insert(child->getUniqueID());
			CollectIDs(child, ids);
		}
	}

	//! Looks for the top-most entities that are not in a set of known IDs
	void CollectNewEntities(ccHObject* obj, const std::set<unsigned>& knownIDs, std::vector<ccHObject*>& newEntities)
	{
		for (unsigned i = 0; i < obj->getChildrenNumber(); ++i)
		{
			ccHObject* child = obj->getChild(i);
			if (knownIDs.count(child->getUniqueID()) == 0)
				newEntities.push_back(child);
			else
				CollectNewEntities(child, knownIDs, newEntities);
		}
	}

	void Detach(ccSleekUndo* undo, std::vector<Holder>& holders)
	{
		MainWindow* win  = undo->win();
		ccHObject*  root = win->dbRootObject();
		for (Holder& h : holders)
		{
			if (h.ptr)
				continue; // already detached
			ccHObject* obj = undo->findInDB(h.id);
			if (!obj)
				continue; // not in the DB anymore
			ccHObject* parent = obj->getParent();
			h.parentId        = (parent && parent != root) ? parent->getUniqueID() : 0;
			obj->prepareDisplayForRefresh_recursive();
			win->removeFromDB(obj, false);
			h.ptr = obj;
		}
	}

	void Attach(ccSleekUndo* undo, std::vector<Holder>& holders)
	{
		MainWindow* win = undo->win();
		for (Holder& h : holders)
		{
			if (!h.ptr)
				continue;
			ccHObject* parent = h.parentId != 0 ? undo->findInDB(h.parentId) : nullptr;
			if (parent)
				parent->addChild(h.ptr);
			win->addToDB(h.ptr, false, true, false, false);
			h.ptr->prepareDisplayForRefresh_recursive();
			h.ptr = nullptr;
		}
	}

	void ApplyFlags(ccSleekUndo* undo, const std::vector<ccSleekUndo::Flags>& flags)
	{
		for (const ccSleekUndo::Flags& f : flags)
		{
			ccHObject* obj = undo->findInDB(f.id);
			if (obj)
			{
				obj->setEnabled(f.enabled);
				obj->setVisible(f.visible);
				obj->prepareDisplayForRefresh_recursive();
			}
		}
	}

	void Refresh(ccSleekUndo* undo)
	{
		ccMainAppInterface* app = undo->win();
		app->refreshAll();
		app->updateUI();
	}

	//! Entities added to / removed from the scene
	class SceneCommand : public QUndoCommand
	{
	  public:
		SceneCommand(ccSleekUndo* undo, const QString& label, bool alreadyDone)
		    : QUndoCommand(label)
		    , m_undo(undo)
		    , m_skipRedo(alreadyDone)
		{
		}

		~SceneCommand() override
		{
			// we own the entities that are currently out of the DB tree
			for (Holder& h : before)
				delete h.ptr;
			for (Holder& h : after)
				delete h.ptr;
		}

		void undo() override
		{
			++m_undo->busy;
			m_undo->win()->db()->unselectAllEntities();
			Detach(m_undo, after);
			Attach(m_undo, before);
			ApplyFlags(m_undo, flagsBefore);
			Refresh(m_undo);
			--m_undo->busy;
		}

		void redo() override
		{
			if (m_skipRedo)
			{
				m_skipRedo = false;
				return;
			}
			++m_undo->busy;
			m_undo->win()->db()->unselectAllEntities();
			Detach(m_undo, before);
			Attach(m_undo, after);
			ApplyFlags(m_undo, flagsAfter);
			Refresh(m_undo);
			--m_undo->busy;
		}

		std::vector<Holder>             before; // entities that only exist before the operation
		std::vector<Holder>             after;  // entities that only exist after the operation
		std::vector<ccSleekUndo::Flags> flagsBefore;
		std::vector<ccSleekUndo::Flags> flagsAfter;

	  private:
		ccSleekUndo* m_undo;
		bool         m_skipRedo;
	};

	//! Rigid transformation
	class TransformCommand : public QUndoCommand
	{
	  public:
		TransformCommand(ccSleekUndo* undo, const QString& label, const std::vector<unsigned>& ids, const ccGLMatrixd& mat, bool alreadyApplied)
		    : QUndoCommand(label)
		    , m_undo(undo)
		    , m_ids(ids)
		    , m_mat(mat)
		    , m_skipRedo(alreadyApplied)
		{
		}

		void undo() override
		{
			apply(m_mat.inverse());
		}

		void redo() override
		{
			if (m_skipRedo)
			{
				m_skipRedo = false;
				return;
			}
			apply(m_mat);
		}

	  private:
		void apply(const ccGLMatrixd& mat)
		{
			++m_undo->busy;
			MainWindow* win = m_undo->win();
			ccGLMatrix  matf(mat.data());
			for (unsigned id : m_ids)
			{
				ccHObject* obj = m_undo->findInDB(id);
				if (!obj)
					continue;
				obj->setGLTransformation(matf);
				// warning: applyGLTransformation may delete the associated octree
				MainWindow::ccHObjectContext context = win->removeObjectTemporarilyFromDBTree(obj);
				obj->applyGLTransformation_recursive();
				obj->prepareDisplayForRefresh_recursive();
				win->putObjectBackIntoDBTree(obj, context);
			}
			Refresh(m_undo);
			--m_undo->busy;
		}

		ccSleekUndo*          m_undo;
		std::vector<unsigned> m_ids;
		ccGLMatrixd           m_mat;
		bool                  m_skipRedo;
	};
} // namespace

ccSleekUndo::ccSleekUndo(MainWindow* win)
    : QObject(win)
    , m_win(win)
{
	m_stack.setUndoLimit(30);
}

ccSleekUndo::~ccSleekUndo()
{
	for (ccHObject* s : m_snapshots)
		delete s;
}

ccHObject* ccSleekUndo::findInDB(unsigned uniqueID) const
{
	return m_win->db() ? m_win->db()->find(static_cast<int>(uniqueID)) : nullptr;
}

void ccSleekUndo::collectIDs(std::set<unsigned>& ids) const
{
	if (m_win->dbRootObject())
		CollectIDs(m_win->dbRootObject(), ids);
}

void ccSleekUndo::clear()
{
	m_stack.clear();
	for (ccHObject* s : m_snapshots)
		delete s;
	m_snapshots.clear();
	m_recording = false;
}

void ccSleekUndo::begin(const QString& label, const ccHObject::Container& affected, bool snapshot)
{
	if (m_recording)
	{
		// operations are not nested
		return;
	}

	m_recording = true;
	m_label     = label;
	m_idsBefore.clear();
	m_flagsBefore.clear();
	collectIDs(m_idsBefore);

	for (ccHObject* entity : affected)
	{
		Flags f;
		f.id      = entity->getUniqueID();
		f.enabled = entity->isEnabled();
		f.visible = entity->isVisible();
		m_flagsBefore.push_back(f);

		if (snapshot && entity->isA(CC_TYPES::POINT_CLOUD))
		{
			ccPointCloud* cloud = static_cast<ccPointCloud*>(entity);
			ccPointCloud* clone = cloud->cloneThis(nullptr, true);
			if (!clone)
			{
				ccLog::Warning(tr("[Undo] Not enough memory to keep a copy of '%1': this step can't be undone").arg(cloud->getName()));
				continue;
			}
			clone->setName(cloud->getName());
			clone->setEnabled(cloud->isEnabled());
			clone->setVisible(cloud->isVisible());
			clone->setDisplay(cloud->getDisplay());
			// the copy takes the place of the original cloud: same identity
			clone->setUniqueID(cloud->getUniqueID());
			// we borrow the meta-data to remember the parent
			ccHObject* parent = cloud->getParent();
			clone->setMetaData("sleek.parentId", (parent && parent != m_win->dbRootObject()) ? parent->getUniqueID() : 0u);
			m_snapshots.push_back(clone);
		}
	}
}

void ccSleekUndo::end()
{
	if (!m_recording)
		return;
	m_recording = false;

	ccHObject* root = m_win->dbRootObject();
	if (!root)
	{
		clear();
		return;
	}

	SceneCommand* cmd = new SceneCommand(this, m_label, true);

	// what has been created?
	std::vector<ccHObject*> newEntities;
	CollectNewEntities(root, m_idsBefore, newEntities);
	for (ccHObject* obj : newEntities)
	{
		Holder h;
		h.id = obj->getUniqueID();
		cmd->after.push_back(h);
	}

	// snapshots (only kept if the original cloud has really changed)
	for (ccHObject* snapshot : m_snapshots)
	{
		ccHObject* current = findInDB(snapshot->getUniqueID());
		bool       changed = true;
		if (current && current->isA(CC_TYPES::POINT_CLOUD))
		{
			changed = (static_cast<ccPointCloud*>(current)->size() != static_cast<ccPointCloud*>(snapshot)->size());
		}
		if (!changed)
		{
			delete snapshot;
			continue;
		}

		Holder before;
		before.id       = snapshot->getUniqueID();
		before.parentId = snapshot->getMetaData("sleek.parentId").toUInt();
		before.ptr      = snapshot;
		snapshot->removeMetaData("sleek.parentId");
		cmd->before.push_back(before);

		if (current)
		{
			Holder after;
			after.id = current->getUniqueID();
			cmd->after.push_back(after);
		}
	}
	m_snapshots.clear();

	// flags
	bool flagsChanged = false;
	for (const Flags& f : m_flagsBefore)
	{
		ccHObject* obj = findInDB(f.id);
		if (!obj)
			continue;
		Flags now;
		now.id      = f.id;
		now.enabled = obj->isEnabled();
		now.visible = obj->isVisible();
		cmd->flagsBefore.push_back(f);
		cmd->flagsAfter.push_back(now);
		if (now.enabled != f.enabled || now.visible != f.visible)
			flagsChanged = true;
	}

	if (cmd->before.empty() && cmd->after.empty() && !flagsChanged)
	{
		// nothing happened
		delete cmd;
		return;
	}

	m_stack.push(cmd);
}

void ccSleekUndo::pushTransform(const QString& label, const ccHObject::Container& entities, const ccGLMatrixd& mat, bool alreadyApplied /*=true*/)
{
	std::vector<unsigned> ids;
	for (ccHObject* entity : entities)
		ids.push_back(entity->getUniqueID());
	if (ids.empty())
		return;
	m_stack.push(new TransformCommand(this, label, ids, mat, alreadyApplied));
}

bool ccSleekUndo::deleteEntities(const ccHObject::Container& entities)
{
	if (entities.empty() || m_recording)
		return false;

	// we don't want any entity that would be the child of another one
	ccHObject::Container topLevel;
	for (ccHObject* entity : entities)
	{
		bool hasSelectedAncestor = false;
		for (ccHObject* other : entities)
		{
			if (other != entity && other->isAncestorOf(entity))
			{
				hasSelectedAncestor = true;
				break;
			}
		}
		if (!hasSelectedAncestor)
			topLevel.push_back(entity);
	}

	// we only handle the simple (and safe) cases
	for (ccHObject* entity : topLevel)
	{
		if (entity->isLocked() || !entity->getParent())
			return false;

		bool supported = entity->isA(CC_TYPES::POINT_CLOUD) || entity->isA(CC_TYPES::MESH) || entity->isA(CC_TYPES::POLY_LINE) || entity->isA(CC_TYPES::HIERARCHY_OBJECT);
		if (!supported)
			return false;

		if (entity->isA(CC_TYPES::POINT_CLOUD))
		{
			ccGenericPointCloud* cloud = static_cast<ccGenericPointCloud*>(entity);
			if (ccGenericMesh::IsCloudVerticesOfMesh(cloud) || ccPolyline::IsCloudVerticesOfPolyline(cloud))
				return false;
		}
	}

	SceneCommand* cmd = new SceneCommand(this, topLevel.size() == 1 ? tr("Delete %1").arg(topLevel.front()->getName()) : tr("Delete %1 items").arg(topLevel.size()), false);
	for (ccHObject* entity : topLevel)
	{
		Holder h;
		h.id = entity->getUniqueID();
		cmd->before.push_back(h);
	}

	// the entities are detached by the first call to 'redo'
	m_stack.push(cmd);
	return true;
}
