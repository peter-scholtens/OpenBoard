/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */



#include "UBPersistenceWorker.h"
#include "adaptors/UBSvgSubsetAdaptor.h"
#include "adaptors/UBThumbnailAdaptor.h"
#include "adaptors/UBMetadataDcSubsetAdaptor.h"

UBPersistenceWorker::UBPersistenceWorker(QObject *parent) :
    QObject(parent)
  , mReceivedApplicationClosing(false)
{
}

void UBPersistenceWorker::saveScene(std::shared_ptr<UBDocumentProxy> proxy, UBGraphicsScene *scene, const int pageIndex)
{
    PersistenceInformation entry = {WriteScene, proxy, scene, pageIndex};

    QMutexLocker locker(&mMutex);
    saves.append(entry);
    mSemaphore.release();
}

void UBPersistenceWorker::saveMetadata(std::shared_ptr<UBDocumentProxy> proxy)
{
    PersistenceInformation entry = {WriteMetadata, proxy, NULL, 0};
    QMutexLocker locker(&mMutex);
    saves.append(entry);
    mSemaphore.release();
}

void UBPersistenceWorker::applicationWillClose()
{
    qDebug() << "application Will close signal received";
    mReceivedApplicationClosing = true;
    mSemaphore.release();
}

void UBPersistenceWorker::process()
{
    qDebug() << "process starts";
    mSemaphore.acquire();
    do{
        PersistenceInformation info;
        {
            QMutexLocker locker(&mMutex);
            info = saves.takeFirst();
        }
        if(info.action == WriteScene){
            UBSvgSubsetAdaptor::persistScene(info.proxy, info.scene->shared_from_this(), info.sceneIndex);
            emit scenePersisted(info.scene);
        }
        else if (info.action == WriteMetadata) {
            UBMetadataDcSubsetAdaptor::persist(info.proxy);
            emit metadataPersisted(info.proxy);
        }
        mSemaphore.acquire();
    }while(!mReceivedApplicationClosing);
    qDebug() << "process will stop";
    emit finished();
}
