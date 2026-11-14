#pragma once

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

// Undo / redo for the "sleek" interface.
//
// CloudCompare has no undo system. This one covers the destructive steps of
// the trim / clip / move / rotate / subsample / topo workflow, with three
// kinds of record:
//  - scene changes: entities that an operation added to or removed from the
//    DB tree (removed entities are kept alive, detached, instead of deleted)
//  - snapshots: a clone taken before an operation that edits a cloud in place
//    (trim)
//  - rigid transformations: only the matrix is stored
//
// Entities that live in the DB tree are referenced by unique ID (never by
// pointer), so a record simply does nothing if its entity has disappeared.

#include <QObject>
#include <QString>
#include <QUndoStack>

#include <ccGLMatrix.h>
#include <ccHObject.h>

#include <set>
#include <vector>

class MainWindow;

class ccSleekUndo : public QObject
{
	Q_OBJECT

  public:
	explicit ccSleekUndo(MainWindow* win);
	~ccSleekUndo() override;

	QUndoStack* stack()
	{
		return &m_stack;
	}

	//! Starts recording an operation
	/** \param label    name shown in the History panel
	    \param affected entities the operation works on
	    \param snapshot whether to clone the affected clouds / meshes first
	                    (needed if the operation edits them in place)
	**/
	void begin(const QString& label, const ccHObject::Container& affected, bool snapshot);

	//! Ends the recording (a History step is created only if the scene changed)
	void end();

	//! Whether a recording is in progress
	bool recording() const
	{
		return m_recording;
	}

	//! Records a rigid transformation that has just been applied
	/** \param alreadyApplied if false, the transformation is applied by this call **/
	void pushTransform(const QString& label, const ccHObject::Container& entities, const ccGLMatrixd& mat, bool alreadyApplied = true);

	//! Removes the entities from the DB tree, in an undoable way
	/** \return false if these entities can't be handled (the caller should fall back to the standard deletion) **/
	bool deleteEntities(const ccHObject::Container& entities);

	//! Forgets everything (to be called when the DB is emptied)
	void clear();

	//! Set while a step is being undone or redone
	int busy = 0;

	// internal helpers (used by the commands)
	MainWindow* win()
	{
		return m_win;
	}
	ccHObject* findInDB(unsigned uniqueID) const;
	void       collectIDs(std::set<unsigned>& ids) const;

	//! State of an entity's flags
	struct Flags
	{
		unsigned id      = 0;
		bool     enabled = true;
		bool     visible = true;
	};

  private:
	MainWindow* m_win;
	QUndoStack  m_stack;

	bool                    m_recording = false;
	QString                 m_label;
	std::set<unsigned>      m_idsBefore;
	std::vector<Flags>      m_flagsBefore;
	std::vector<ccHObject*> m_snapshots; // clones (same unique ID as their source)
};

//! RAII helper: records whatever happens to the selection during its lifetime
class ccSleekUndoScope
{
  public:
	ccSleekUndoScope(ccSleekUndo* undo, const QString& label, const ccHObject::Container& affected, bool snapshot = false)
	    : m_undo(undo && !undo->recording() ? undo : nullptr)
	{
		if (m_undo)
			m_undo->begin(label, affected, snapshot);
	}
	~ccSleekUndoScope()
	{
		if (m_undo)
			m_undo->end();
	}

  private:
	ccSleekUndo* m_undo;
};
