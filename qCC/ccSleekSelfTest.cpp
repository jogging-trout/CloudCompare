// ##########################################################################
// #                                                                        #
// #                              CLOUDCOMPARE                              #
// #                                                                        #
// #  This program is free software; you can redistribute it and/or modify  #
// #  it under the terms of the GNU General Public License as published by  #
// #  the Free Software Foundation; version 2 or later of the License.      #
// #                                                                        #
// ##########################################################################

// Automated smoke test of the sleek interface.
// Run with: CC_SLEEK_SELFTEST=<output folder> CloudCompare <cloud file>
// It drives the main operations, checks undo / redo, saves screenshots and quits.

#include "ccSleekUI.h"

#include "ccDBRoot.h"
#include "ccGraphicalSegmentationTool.h"
#include "ccSleekUndo.h"
#include "mainwindow.h"

#include "ui_mainWindow.h"

#include <ccPointCloud.h>

#include <FileIOFilter.h>

#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QScreen>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <cstdio>
#include <functional>

namespace
{
	int s_failures = 0;

	void Check(bool ok, const QString& what)
	{
		if (!ok)
			++s_failures;
		std::printf("[SELFTEST] %s: %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
		std::fflush(stdout);
	}

	void Shot(const QString& name)
	{
		QApplication::processEvents();
		QString file = QDir(qEnvironmentVariable("CC_SLEEK_SELFTEST")).filePath(name + ".png");
		QApplication::primaryScreen()->grabWindow(0).save(file);
	}

	unsigned CountEntities(MainWindow* win, CC_CLASS_ENUM type)
	{
		ccHObject::Container found;
		win->dbRootObject()->filterChildren(found, true, type, true);
		return static_cast<unsigned>(found.size());
	}
} // namespace

void ccSleekUI::runSelfTest()
{
	// the steps are spread over time so that the views get repainted in between
	QList<std::function<void()>>* steps = new QList<std::function<void()>>;
	QUndoStack*                   stack = m_undo->stack();

	static ccPointCloud* s_cloud  = nullptr;
	static unsigned      s_size   = 0;
	static CCVector3     s_center;

	auto firstCloud = [this]() -> ccPointCloud*
	{
		ccHObject::Container clouds;
		m_win->dbRootObject()->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD, true);
		for (ccHObject* c : clouds)
			if (c->isBranchEnabled() && c->getName() != "vertices")
				return static_cast<ccPointCloud*>(c);
		return nullptr;
	};

	steps->append([=]()
	              { m_win->setGeometry(QApplication::primaryScreen()->geometry()); });
	steps->append([=]()
	              {
		              {
			              FileIOFilter::LoadParameters params;
			              params.alwaysDisplayLoadDialog = false;
			              params.parentWidget            = m_win;
			              CC_FILE_ERROR result           = CC_FERR_NO_ERROR;
			              ccHObject*    loaded           = FileIOFilter::LoadFromFile(qEnvironmentVariable("CC_SLEEK_SELFTEST_FILE"), params, result);
			              if (loaded)
				              m_win->addToDB(loaded, true, true, false, true);
		              }
		              s_cloud = firstCloud();
		              Check(s_cloud != nullptr, "a cloud is loaded");
		              if (!s_cloud)
		              {
			              steps->clear();
			              QApplication::exit(2);
			              return;
		              }
		              s_size   = s_cloud->size();
		              s_center = s_cloud->getOwnBB().getCenter();
		              m_ui->actionSetViewIso1->trigger();
		              m_ui->actionGlobalZoom->trigger();
		              Shot("01_light_cloud"); });

	// rotate + undo
	steps->append([=]()
	              {
		              m_win->db()->selectEntity(s_cloud);
		              QApplication::processEvents();
		              rotateBy(2, 90.0);
		              Check(stack->count() == 1, "rotation is recorded");
		              CCVector3 size = s_cloud->getOwnBB().getDiagVec();
		              std::printf("[SELFTEST] bbox after rotation: %.2f x %.2f x %.2f\n", size.x, size.y, size.z);
		              stack->undo();
		              CCVector3 c = s_cloud->getOwnBB().getCenter();
		              Check((c - s_center).norm() < 1.0e-3, "undo restores the position after a rotation");
		              moveBy(10, 0, 0);
		              c = s_cloud->getOwnBB().getCenter();
		              Check(std::abs(c.x - s_center.x - 10) < 1.0e-3, "move by 10 m along X");
		              stack->undo();
		              c = s_cloud->getOwnBB().getCenter();
		              Check((c - s_center).norm() < 1.0e-3, "undo restores the position after a move"); });

	// subsample
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              QApplication::processEvents();
		              ccHObject::Container out = subsample(1.0);
		              Check(out.size() == 1 && static_cast<ccPointCloud*>(out.front())->size() < s_size, "subsample creates a lighter cloud");
		              Check(!s_cloud->isEnabled(), "the original cloud is hidden");
		              stack->undo();
		              Check(CountEntities(m_win, CC_TYPES::POINT_CLOUD) == 1 && s_cloud->isEnabled(), "undo removes the subsampled cloud and shows the original");
		              stack->redo();
		              Check(CountEntities(m_win, CC_TYPES::POINT_CLOUD) == 2 && !s_cloud->isEnabled(), "redo brings the subsampled cloud back");
		              stack->undo(); });

	// contours
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              QApplication::processEvents();
		              ccHObject::Container lines = contours(1.0);
		              Check(lines.size() == 1 && lines.front()->getChildrenNumber() > 3, "1 m topo lines are generated");
		              if (!lines.empty())
		              {
			              std::printf("[SELFTEST] %u polylines\n", lines.front()->getChildrenNumber());
			              Check(exportForSketchUp(lines, "dxf", "selftest_topo_1m"), "DXF export");
		              }
		              m_ui->actionSetViewTop->trigger();
		              m_ui->actionGlobalZoom->trigger(); });
	steps->append([=]()
	              { Shot("02_light_contours_top"); });
	steps->append([=]()
	              {
		              unsigned polyCount = CountEntities(m_win, CC_TYPES::POLY_LINE);
		              stack->undo();
		              Check(polyCount > 0 && CountEntities(m_win, CC_TYPES::POLY_LINE) == 0, "undo removes the topo lines"); });

	// workflow (subsample + contours) = one history step
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              QApplication::processEvents();
		              int before = stack->index();
		              workflowTopo(0.5, 5.0, false);
		              Check(stack->index() == before + 1, "a workflow is a single history step");
		              Check(CountEntities(m_win, CC_TYPES::POLY_LINE) > 0, "workflow: 5 m topo lines");
		              stack->undo();
		              Check(CountEntities(m_win, CC_TYPES::POLY_LINE) == 0 && CountEntities(m_win, CC_TYPES::POINT_CLOUD) == 1 && s_cloud->isEnabled(), "undo reverts the whole workflow"); });

	// mesh workflow
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              QApplication::processEvents();
		              workflowMesh(1.0, true);
		              Check(CountEntities(m_win, CC_TYPES::MESH) == 1, "workflow: mesh");
		              m_ui->actionSetViewIso1->trigger();
		              m_ui->actionGlobalZoom->trigger(); });
	steps->append([=]()
	              { Shot("03_light_mesh"); });
	steps->append([=]()
	              {
		              stack->undo();
		              Check(CountEntities(m_win, CC_TYPES::MESH) == 0 && s_cloud->isEnabled(), "undo reverts the mesh workflow"); });

	// delete + undo
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              m_win->db()->selectEntity(s_cloud);
		              QApplication::processEvents(); });
	steps->append([=]()
	              {
		              m_win->db()->deleteSelectedEntities();
		              Check(CountEntities(m_win, CC_TYPES::POINT_CLOUD) == 0, "delete removes the cloud");
		              Check(stack->canUndo(), "the history survives an emptied scene");
		              stack->undo();
		              Check(CountEntities(m_win, CC_TYPES::POINT_CLOUD) == 1, "undo restores the deleted cloud"); });

	// panels
	steps->append([=]()
	              {
		              contours(1.0);
		              m_ui->actionSetViewIso1->trigger();
		              m_ui->actionGlobalZoom->trigger();
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actTopo)))
			              button->menu()->popup(button->mapToGlobal(QPoint(button->width() + 4, 0))); });
	steps->append([=]()
	              {
		              Shot("04_light_contours_panel");
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actTopo)))
			              button->menu()->close();
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actRotate)))
			              button->menu()->popup(button->mapToGlobal(QPoint(button->width() + 4, 0))); });
	steps->append([=]()
	              {
		              Shot("05_light_rotate_panel");
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actRotate)))
			              button->menu()->close();
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actFlows)))
			              button->menu()->popup(button->mapToGlobal(QPoint(button->width() + 4, 0))); });
	steps->append([=]()
	              {
		              Shot("06_light_workflows_menu");
		              if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actFlows)))
			              button->menu()->close();
		              setTheme(DARK); });
	steps->append([=]()
	              { Shot("07_dark"); });

	// trim tool (dark mode)
	steps->append([=]()
	              {
		              m_win->db()->unselectAllEntities();
		              m_win->db()->selectEntity(s_cloud);
		              QApplication::processEvents(); });
	steps->append([=]()
	              { m_actTrim->trigger(); });
	steps->append([=]()
	              {
		              Shot("08_dark_trim_tool");
		              Check(m_win->m_gsTool != nullptr && !m_actSelect->isEnabled(), "the trim tool starts from the rail");
		              if (m_win->m_gsTool)
			              m_win->m_gsTool->stop(false); });
	steps->append([=]()
	              {
		              Check(m_actSelect->isEnabled(), "the rail is released when the tool stops");
		              m_ui->menuEdit->popup(m_win->mapToGlobal(QPoint(60, 30))); });
	steps->append([=]()
	              {
		              Shot("09_dark_edit_menu");
		              m_ui->menuEdit->close();
		              setTheme(LIGHT);
		              std::printf("[SELFTEST] DONE, %d failure(s)\n", s_failures);
		              std::fflush(stdout);
		              QApplication::exit(s_failures == 0 ? 0 : 1); });

	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [=]()
	        {
		        if (steps->isEmpty())
		        {
			        timer->stop();
			        return;
		        }
		        std::function<void()> step = steps->takeFirst();
		        step(); });
	timer->start(900);
}
