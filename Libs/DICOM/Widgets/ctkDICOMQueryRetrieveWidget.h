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

#ifndef __ctkDICOMQueryRetrieveWidget_h
#define __ctkDICOMQueryRetrieveWidget_h

#include "ctkDICOMWidgetsExport.h"

// Qt includes 
#include <QWidget>


// CTK includes
#include <ctkDICOMDatabase.h>

class ctkDICOMQueryRetrieveWidgetPrivate;

class CTK_DICOM_WIDGETS_EXPORT ctkDICOMQueryRetrieveWidget : public QWidget
{
Q_OBJECT;
public:
  typedef QWidget Superclass;
  explicit ctkDICOMQueryRetrieveWidget(QWidget* parent=0);
  virtual ~ctkDICOMQueryRetrieveWidget();

public slots:
  void setRetrieveDatabase(QSharedPointer<ctkDICOMDatabase> retrieveDatabase);
  void processQuery();
  void processRetrieve();

protected slots:
  void onQueryProgressChanged(int value);

protected:
  QScopedPointer<ctkDICOMQueryRetrieveWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMQueryRetrieveWidget);
  Q_DISABLE_COPY(ctkDICOMQueryRetrieveWidget);
};

#endif
