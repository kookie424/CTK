/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.commontk.org/LICENSE

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

//Qt includes
#include <QDebug>
#include <QLabel>
#include <QProgressDialog>
#include <QSettings>
#include <QTreeView>
#include <QTabBar>

/// CTK includes
#include <ctkCheckableHeaderView.h>
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMModel.h"
#include "ctkDICOMQuery.h"
#include "ctkDICOMRetrieve.h"

// ctkDICOMWidgets includes
#include "ctkDICOMQueryRetrieveWidget.h"
#include "ctkDICOMQueryResultsTabWidget.h"
#include "ui_ctkDICOMQueryRetrieveWidget.h"

static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMQueryRetrieveWidget");

//----------------------------------------------------------------------------
class ctkDICOMQueryRetrieveWidgetPrivate: public Ui_ctkDICOMQueryRetrieveWidget
{
public:
  ctkDICOMQueryRetrieveWidgetPrivate(){}

  QMap<QString, ctkDICOMQuery*> QueriesByServer;
  QMap<QString, ctkDICOMQuery*> QueriesByStudyUID;
  QMap<QString, ctkDICOMRetrieve*> RetrievalsByStudyUID;
  ctkDICOMDatabase QueryResultDatabase;
  QSharedPointer<ctkDICOMDatabase> RetrieveDatabase;
  ctkDICOMModel    model;
  
  QProgressDialog* ProgressDialog;
  QString          CurrentServer;
};

//----------------------------------------------------------------------------
// ctkDICOMQueryRetrieveWidgetPrivate methods

//----------------------------------------------------------------------------
// ctkDICOMQueryRetrieveWidget methods

//----------------------------------------------------------------------------
ctkDICOMQueryRetrieveWidget::ctkDICOMQueryRetrieveWidget(QWidget* parentWidget)
  : Superclass(parentWidget) 
  , d_ptr(new ctkDICOMQueryRetrieveWidgetPrivate)
{
  Q_D(ctkDICOMQueryRetrieveWidget);
  
  d->setupUi(this);

  d->ProgressDialog = 0;
  connect(d->QueryButton, SIGNAL(clicked()), this, SLOT(processQuery()));
  connect(d->RetrieveButton, SIGNAL(clicked()), this, SLOT(processRetrieve()));
  connect(d->CancelButton, SIGNAL(clicked()),this,SLOT(hide()));

  d->results->setModel(&d->model);
  d->model.setHeaderData(0, Qt::Horizontal, Qt::Unchecked, Qt::CheckStateRole);

  QHeaderView* previousHeaderView = d->results->header();
  ctkCheckableHeaderView* headerView = new ctkCheckableHeaderView(Qt::Horizontal, d->results);
  headerView->setClickable(previousHeaderView->isClickable());
  headerView->setMovable(previousHeaderView->isMovable());
  headerView->setHighlightSections(previousHeaderView->highlightSections());
  headerView->setPropagateToItems(true);
  d->results->setHeader(headerView);
  // headerView is hidden because it was created with a visisble parent widget 
  headerView->setHidden(false);
}

//----------------------------------------------------------------------------
ctkDICOMQueryRetrieveWidget::~ctkDICOMQueryRetrieveWidget()
{
}

//----------------------------------------------------------------------------
void ctkDICOMQueryRetrieveWidget::setRetrieveDatabase(QSharedPointer<ctkDICOMDatabase> dicomDatabase)
{
  Q_D(ctkDICOMQueryRetrieveWidget);

  d->RetrieveDatabase = dicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryRetrieveWidget::processQuery()
{
  Q_D(ctkDICOMQueryRetrieveWidget);

  d->RetrieveButton->setEnabled(false);
  
  // create a database in memory to hold query results
  try { d->QueryResultDatabase.openDatabase( ":memory:", "QUERY-DB" ); }
  catch (std::exception e)
  {
    logger.error ( "Database error: " + d->QueryResultDatabase.lastError() );
    d->QueryResultDatabase.closeDatabase();
    return;
  }

  // for each of the selected server nodes, send the query
  QProgressDialog progress("Query DICOM servers", "Cancel", 0, 100, this,
                           Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
  // We don't want the progress dialog to resize itself, so we bypass the label
  // by creating our own
  QLabel* progressLabel = new QLabel("Initialization...");
  progress.setLabel(progressLabel);
  d->ProgressDialog = &progress;
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);
  progress.setValue(0);
  foreach (d->CurrentServer, d->ServerNodeWidget->checkedNodes())
    {
    if (progress.wasCanceled())
      {
      break;
      }
    QMap<QString, QVariant> parameters = d->ServerNodeWidget->nodeParameters(d->CurrentServer);
    // if we are here it's because the server node was checked
    Q_ASSERT(parameters["CheckState"] == Qt::Checked );
    // create a query for the current server
    ctkDICOMQuery* query = new ctkDICOMQuery;
    d->QueriesByServer[d->CurrentServer] = query;
    query->setCallingAETitle(d->ServerNodeWidget->callingAETitle());
    query->setCalledAETitle(parameters["AETitle"].toString());
    query->setHost(parameters["Address"].toString());
    query->setPort(parameters["Port"].toInt());

    // populate the query with the current search options
    query->setFilters( d->QueryWidget->parameters() );

    try
      {
      connect(query, SIGNAL(progress(QString)),
              //&progress, SLOT(setLabelText(QString)));
              progressLabel, SLOT(setText(QString)));
      // for some reasons, setLabelText() doesn't refresh the dialog.
      connect(query, SIGNAL(progress(int)),
              this, SLOT(onQueryProgressChanged(int)));
      // run the query against the selected server and put results in database
      query->query ( d->QueryResultDatabase );
      disconnect(query, SIGNAL(progress(QString)),
                 //&progress, SLOT(setLabelText(QString)));
                 progressLabel, SLOT(setText(QString)));
      disconnect(query, SIGNAL(progress(int)),
                 this, SLOT(onQueryProgressChanged(int)));
      }
    catch (std::exception e)
      {
      logger.error ( "Query error: " + parameters["Name"].toString() );
      progress.setLabelText("Query error: " + parameters["Name"].toString());
      }

    foreach( QString studyUID, query->studyInstanceUIDQueried() )
      {
      d->QueriesByStudyUID[studyUID] = query;
      }
    }
  
  // checkable headers - allow user to select the patient/studies to retrieve
  d->model.setDatabase(d->QueryResultDatabase.database());

  d->RetrieveButton->setEnabled(d->model.rowCount());
  progress.setValue(progress.maximum());
  d->ProgressDialog = 0;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryRetrieveWidget::processRetrieve()
{
  Q_D(ctkDICOMQueryRetrieveWidget);

  QMap<QString,QVariant> serverParameters = d->ServerNodeWidget->parameters();

  foreach( QString studyUID, d->QueriesByStudyUID.keys() )
  {
    logger.debug("need to retrieve " + studyUID + " from " + d->QueriesByStudyUID[studyUID]->host());
    ctkDICOMQuery *query = d->QueriesByStudyUID[studyUID];
    ctkDICOMRetrieve *retrieve = new ctkDICOMRetrieve;
    retrieve->setRetrieveDatabase( d->RetrieveDatabase );
    d->RetrievalsByStudyUID[studyUID] = retrieve;
    retrieve->setCallingAETitle( query->callingAETitle() );
    retrieve->setCalledAETitle( query->calledAETitle() );
    retrieve->setCalledPort( query->port() );
    retrieve->setHost( query->host() );

    // pull from GUI
    retrieve->setMoveDestinationAETitle( serverParameters["StorageAETitle"].toString() );
    retrieve->setCallingPort( serverParameters["StoragePort"].toInt() );

    logger.info ( "Starting to retrieve" );
    try
      {
      retrieve->retrieveStudy ( studyUID );
      }
    catch (std::exception e)
      {
      logger.error ( "Retrieve failed" );
      return;
      }
    logger.info ( "Retrieve success" );
  }
}

//----------------------------------------------------------------------------
void ctkDICOMQueryRetrieveWidget::onQueryProgressChanged(int value)
{
  Q_D(ctkDICOMQueryRetrieveWidget);
  if (d->ProgressDialog == 0)
    {
    return;
    }
  QStringList servers = d->ServerNodeWidget->checkedNodes();
  int serverIndex = servers.indexOf(d->CurrentServer);
  if (serverIndex < 0)
    {
    return;
    }
  float serverProgress = 100. / servers.size();
  d->ProgressDialog->setValue( (serverIndex + (value / 101.)) * serverProgress);
  if (d->ProgressDialog->width() != 500)
    {
    QPoint pp = this->mapToGlobal(QPoint(0,0));
    pp = QPoint(pp.x() + (this->width() - d->ProgressDialog->width()) / 2,
                pp.y() + (this->height() - d->ProgressDialog->height())/ 2);
    d->ProgressDialog->move(pp - QPoint((500 - d->ProgressDialog->width())/2, 0));
    d->ProgressDialog->resize(500, d->ProgressDialog->height());
    }
  //d->CurrentServerqApp->processEvents();
}
