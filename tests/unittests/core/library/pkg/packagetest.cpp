/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <gtest/gtest.h>
#include <librepcb/core/application.h>
#include <librepcb/core/fileio/fileutils.h>
#include <librepcb/core/fileio/transactionalfilesystem.h>
#include <librepcb/core/library/pkg/package.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace tests {

/*******************************************************************************
 *  Test Class
 ******************************************************************************/

class PackageTest : public ::testing::Test {
protected:
  FilePath mTmpDir;
  static constexpr const char* sUuid = "da9e3bd5-7c56-4d6c-987c-603220599356";

  PackageTest() {
    // the whitespaces in the path are there to make the test even stronger ;)
    mTmpDir = FilePath::getRandomTempPath().getPathTo(sUuid);
  }

  virtual ~PackageTest() {
    QDir(mTmpDir.getParentDir().toStr()).removeRecursively();
  }

  std::unique_ptr<TransactionalDirectory> createDir(
      bool writable = true) const noexcept {
    return std::unique_ptr<TransactionalDirectory>(new TransactionalDirectory(
        TransactionalFileSystem::open(mTmpDir, writable)));
  }
};

/*******************************************************************************
 *  Test Methods
 ******************************************************************************/

TEST_F(PackageTest, testUpgradeV01) {
  // Copy into temporary directory.
  const FilePath src =
      FilePath(TEST_DATA_DIR "/libraries/v0.1.lplib/pkg").getPathTo(sUuid);
  FileUtils::copyDirRecursively(src, mTmpDir);

  // Open/upgrade/close.
  ASSERT_TRUE(FileUtils::readFile(mTmpDir.getPathTo(".librepcb-pkg"))
                  .startsWith("0.1\n"));
  {
    std::unique_ptr<Package> obj = Package::open(createDir());
    obj->save();
    obj->getDirectory().getFileSystem()->save();
  }

  // Re-open.
  ASSERT_TRUE(
      FileUtils::readFile(mTmpDir.getPathTo(".librepcb-pkg"))
          .startsWith(Application::getFileFormatVersion().toStr().toUtf8() %
                      "\n"));
  { std::unique_ptr<Package> obj = Package::open(createDir()); }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace tests
}  // namespace librepcb
