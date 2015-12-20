/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 Urban Bruhin
 * http://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/

#include <QtCore>
#include "device.h"
#include <librepcbcommon/fileio/xmldomelement.h>

namespace library {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

Device::Device(const QUuid& uuid, const Version& version, const QString& author,
               const QString& name_en_US, const QString& description_en_US,
               const QString& keywords_en_US) throw (Exception) :
    LibraryElement("dev", "device", uuid, version, author, name_en_US, description_en_US, keywords_en_US)
{
    Q_ASSERT(mUuid.isNull() == false);
}

Device::Device(const FilePath& elementDirectory) throw (Exception) :
    LibraryElement(elementDirectory, "dev", "device")
{
    readFromFile();
}

Device::~Device() noexcept
{
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void Device::parseDomTree(const XmlDomElement& root) throw (Exception)
{
    LibraryElement::parseDomTree(root);

    if (root.getFirstChild("meta/generic_component", true, false)) {
        // TODO: remove this
        mComponentUuid = root.getFirstChild("meta/generic_component", true, true)->getText<QUuid>(true);
    } else {
        mComponentUuid = root.getFirstChild("meta/component", true, true)->getText<QUuid>(true);
    }
    mPackageUuid = root.getFirstChild("meta/package", true, true)->getText<QUuid>(true);
    for (XmlDomElement* node = root.getFirstChild("pad_signal_map/map", true, false);
         node; node = node->getNextSibling("map"))
    {
        if (node->hasAttribute("signal")) {
            // TODO: remove this
            mPadSignalMap.insert(node->getAttribute<QUuid>("pad", true),
                                 node->getAttribute<QUuid>("signal", false));
        } else {
            mPadSignalMap.insert(node->getAttribute<QUuid>("pad", true),
                                 node->getText<QUuid>(false, QUuid()));
        }
    }
}

XmlDomElement* Device::serializeToXmlDomElement() const throw (Exception)
{
    QScopedPointer<XmlDomElement> root(LibraryElement::serializeToXmlDomElement());
    root->getFirstChild("meta", true)->appendTextChild("component", mComponentUuid);
    root->getFirstChild("meta", true)->appendTextChild("package", mPackageUuid);
    XmlDomElement* padSignalMap = root->appendChild("pad_signal_map");
    foreach (const QUuid& padUuid, mPadSignalMap.keys())
    {
        XmlDomElement* child = padSignalMap->appendChild("map");
        child->setAttribute("pad", padUuid);
        child->setText(mPadSignalMap.value(padUuid).isNull() ? QString() : mPadSignalMap.value(padUuid).toString());
    }
    return root.take();
}

bool Device::checkAttributesValidity() const noexcept
{
    if (!LibraryElement::checkAttributesValidity())             return false;
    if (mComponentUuid.isNull())                                return false;
    if (mPackageUuid.isNull())                                  return false;
    foreach (const QUuid& padUuid, mPadSignalMap.keys())
    {
        if (padUuid.isNull())                                   return false;
    }
    return true;
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace library
