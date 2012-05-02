#include <iostream>
#include <fstream>

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QThreadPool>
#include <QtConcurrentMap>

#include "FileName.h"
#include "IException.h"
#include "Preference.h"
#include "ProgramLauncher.h"

using namespace std;
using namespace Isis;

void TestVersioning(iString prefix, iString name, bool containsDate = false);
void TestExpanded(iString prefix, iString name);
void TestExtensionChanges(iString prefix, iString name, bool showExpandedValues);
void TestGenericAccessors(iString prefix, iString name, bool showExpandedValues);

int main(int argc, char *argv[]) {
  Preference::Preferences(true);

  QCoreApplication app(argc, argv);

  QStringList filesToFullTest;
  filesToFullTest << "/path/base.ext+attr" << "/path1/.path2/base.ext+attr"
                  << "/path1/pat.h2/base+attr" << "/.path1/path2/base"
                  << "/.path1/path2/base.+attr" << "/another/path/base.ex1.exten2.ext3"
                  << "/$BADENV/base.ext+attr" << "unitTest.cpp" << "./unitTest.cpp"
                  << "Makefile" << ".cub" << "/$TEMPORARY/unitTest.cpp";

  foreach (QString fileToTest, filesToFullTest) {
    cout << "Running Full Test on [" << qPrintable(fileToTest) << "]" << endl;
    TestGenericAccessors("\t", fileToTest, true);
    TestExtensionChanges("\t", fileToTest, true);
    TestExpanded("\t", fileToTest);
  }

  // Variable expansion will change truth data on these
  QStringList filesToTestSafely;
  filesToTestSafely << "$base/testData/isisTruth.cub" << "${base}/testData/isisTruth.cub"
                    << "$ISISROOT/src/Makefile" << "$ISISROOT/src/Makefile.elifekaM"
                    << "/$TEMPORARY/unitTest.cpp";

  foreach (QString fileToTest, filesToTestSafely) {
    cout << "Running Safe Test on [" << qPrintable(fileToTest) << "]" << endl;
    TestGenericAccessors("\t", fileToTest, false);
    TestExtensionChanges("\t", fileToTest, false);
  }

  // Test temp files thoroughly
  cout << "Testing temporary file name placement" << endl;
  iString tempFileNameTestStr = "$TEMPORARY/tttt.tmp";
  FileName n = FileName::createTempFile(tempFileNameTestStr);
  cout << "\tInput name and extension : " << tempFileNameTestStr << endl;
  cout << "\tExtension:               : " << n.extension() << endl;
  cout << "\tOriginal Path:           : " << n.originalPath() << endl;
  cout << "\tExists:                  : " << n.fileExists() << endl;
  cout << "\tName (cleaned):          : " <<
      iString(n.name().ToQt().mid(0, 4) +
              QString("%1").arg("", n.name().size() - 8, '?') +
              n.name().ToQt().mid(n.name().size() - 4)) << endl;
  cout << endl;

  if (!QFile(n.toString()).remove()) {
    cout << "\t**Failed to delete temporary file [" << n.toString() << "]**" << endl;
  }

  {
    cout << "Testing parallel temporary file name creation for atomicity" << endl;
    int numToTest = QThreadPool::globalInstance()->maxThreadCount() * 20;

    QStringList templateFileNames;
    for (int i = 0; i < numToTest; i++)
      templateFileNames.append(QString("tttt.tmp"));

    QList<FileName> results = QtConcurrent::blockingMapped(templateFileNames,
                                                           &FileName::createTempFile);
    bool success = true;

    while (results.count()) {
      FileName result = results.first();
      int count = results.removeAll(result);

      if (count != 1) {
        cout << "File name: " << result.toString() << " encountered "
             << count << " times" << endl;
        success = false;
      }

      if (!result.fileExists()) {
        cout << "File name: " << result.toString() << " encountered does not exists";
        success = false;
      }

      QFile(result.toString()).remove();
    }

    if (success) {
      cout << "\tSuccess!" << endl;
    }
    else {
      cout << "\t**Failed to uniquely create temporary files in parallel**" << endl;
    }

    cout << endl;
  }


  // Test versioning thoroughly
  {
    QStringList tempFiles;
    tempFiles << "tttt000001" << "tttt000001.tmp" << "tttt000005.tmp"  << "tttt000006.tmp"
              << "tttt000008.tmp" << "1tttt000008.tmp" << "2tttt000008.tmp"
              << "tttt_0.tmp" << "junk06.tmp" << "junk09.tmp" << "tttt05Sep2002.tmp"
              << "tttt20Jan2010.tmp" << "tttt14Apr2010.tmp" << "ttAPRtt22yy99.tmp"
              << "ttMARtt11yy00.tmp" << "ttFEBtt04yy01.tmp" << "ttMARtt072003.tmp"
              << "tt14ttNovember.tmp" << "tt2ttDecember.tmp" << "tttt.tmp" << "APR-22-99_v001.tmp"
              << "APR-22-99_v004.tmp" << "APR-21-99_v009.tmp";

    foreach (QString fileName, tempFiles) {
      if (!QFile(fileName).open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cout << "Failed to create temporary file for test: " << qPrintable(fileName) << endl;
      }
    }


    cout << "Testing Versioning Missing Problems" << endl;
    TestVersioning("\t", "tttt");
    TestVersioning("\t", "tttt{}.tmp");
    TestVersioning("\t", "ttttt{}.tmp");

    cout << "Testing Numerical-Only Versioning" << endl;
    TestVersioning("\t", "tttt??????");
    TestVersioning("\t", "tttt??????.tmp");
    TestVersioning("\t", "tttt_?.tmp");
    TestVersioning("\t", "??tttt");
    TestVersioning("\t", "?tttt000008.tmp");
    TestVersioning("\t", "junk?");
    TestVersioning("\t", "??tttt??");


    cout << "Testing Date-Only Versioning" << endl;
    TestVersioning("\t", "tttt{ddMMMyyyy}.tmp", true);
    TestVersioning("\t", "tt{MMM}tt{dd}yy{yy}.tmp", true);
    TestVersioning("\t", "tt{d}tt{MMM}.tmp", true);
    TestVersioning("\t", "tt{d}tt{MMMM}.tmp", true);
    TestVersioning("\t", "tt{dd}.tmp", true);
    TestVersioning("\t", "tttt{dd}.tmp", true);
    TestVersioning("\t", "tttt{aaaa}.tmp");


    cout << "Testing Date and Numerical Versioning Combined" << endl;
    TestVersioning("\t", "$TEMPORARY/{MMM}-{dd}-{yy}_v???.tmp", true);

    // Test new version for date file.  Makes a file for today, which cannot be
    // printed as truth, so compare it with what we expect and print the result.
    FileName todayFileName("tttt{dd}tt{yyyy}tt{MMM}.tmp");
    cout << "Verifying NewVersion for file " << todayFileName.name() << " is today" << endl;
    bool success = false;
    try {
      todayFileName = todayFileName.newVersion();
      QDate today = QDate::currentDate();
      QString expected = today.toString("'tttt'dd'tt'yyyy'tt'MMM'.tmp'");
      success = todayFileName.name() == expected.toStdString();
      cout << "\tMade today's filename successfully? " << success << endl;

      if (!success)
        cout << "\t\tMade: " << todayFileName.name() << "; expected: " << expected.toStdString()
             << endl;
    }
    catch(IException &error) {
      error.print();
    }
    cout << endl;

    foreach (QString fileName, tempFiles) {
      if (!QFile(fileName).remove()) {
        cout << "Failed to delete temporary file for test: " << qPrintable(fileName) << endl;
        cout << "Was it specified twice?" << endl;
      }
    }
  }

  return 0;
}


void TestVersioning(iString prefix, iString name, bool containsDate) {
  cout << prefix << "Testing Versioning Methods [" << name << "]" << endl;

  try {
    FileName test(name);

    try {
      cout << prefix << "\tHighest Version Name:       "
           << test.highestVersion().name() << endl;
      cout << prefix << "\tHighest Version Orig:       "
           << test.highestVersion().original() << endl;
      cout << prefix << "\tHighest Version Orig Path:  "
           << test.highestVersion().originalPath() << endl;
    }
    catch (IException &e) {
      cout << prefix << "\tHighest Version Failed:     " << e.toString() << endl;
    }

    try {
      if (!containsDate) {
        cout << prefix << "\tNew Version Name:           "
             << test.newVersion().name() << endl;
        cout << prefix << "\tNew Version Orig:           "
             << test.newVersion().original() << endl;
        cout << prefix << "\tNew Version Orig Path:      "
             << test.newVersion().originalPath() << endl;
      }
    }
    catch (IException &e) {
      cout << prefix << "\tNew Version Failed:         " << e.toString() << endl;
    }
  }
  catch(IException &e) {
    cout << prefix << "\tError creating file name: " << e.toString() << endl;
  }

  cout << endl;
}


void TestGenericAccessors(iString prefix, iString name, bool showExpandedValues) {
  FileName a(name);

  // Test our assignment & copy construct every time
  FileName b;
  b = a;
  FileName c(b);
  FileName test;
  test = c;

  cout << prefix << "Testing Basics [" << name << "]" << endl;
  cout << prefix << "\tName:              " << test.name() << endl;
  cout << prefix << "\tBase Name:         " << test.baseName() << endl;

  if (showExpandedValues)
    cout << prefix << "\tPath:              " << test.path() << endl;

  cout << prefix << "\tOriginal path      " << test.originalPath() << endl;

  cout << prefix << "\tExtension:         " << test.extension() << endl;

  if (showExpandedValues)
    cout << prefix << "\tExpanded           " << test.expanded() << endl;

  cout << prefix << "\tExists             " << test.fileExists() << endl;
  cout << endl;
}


void TestExtensionChanges(iString prefix, iString name, bool showExpandedValues) {
  FileName a(name);

  // Test our assignment & copy construct every time
  FileName b;
  b = a;
  FileName c(b);
  FileName test;
  test = c;

  iString (FileName::*toStringMethod)() const = &FileName::toString;

  if (!showExpandedValues)
    toStringMethod = &FileName::original;

  cout << prefix << "Testing Extension change [" << name << "]" << endl;
  cout << prefix << "\tBefore modification:      " << (test.*toStringMethod)() << endl;
  test = test.removeExtension();
  cout << prefix << "\tRemoved Extension:        " << (test.*toStringMethod)() << endl;
  test = test.addExtension("tmp");
  cout << prefix << "\tAdded Extension [tmp]:    " << (test.*toStringMethod)() << endl;
  test = test.addExtension("jpg");
  cout << prefix << "\tAdded Extension [jpg]:    " << (test.*toStringMethod)() << endl;
  test = test.addExtension("jpg");
  cout << prefix << "\tAdded Extension [jpg]:    " << (test.*toStringMethod)() << endl;
  test = test.setExtension("gif");
  cout << prefix << "\tSet Extension   [gif]:    " << (test.*toStringMethod)() << endl;
  test = test.addExtension("jpg");
  cout << prefix << "\tAdded Extension [jpg]:    " << (test.*toStringMethod)() << endl;
  test = test.removeExtension();
  cout << prefix << "\tRemoved Extension:        " << (test.*toStringMethod)() << endl;
  cout << endl;
}


void TestExpanded(iString prefix, iString name) {
  FileName a(name);

  // Test our assignment & copy construct every time
  FileName b;
  b = a;
  FileName c(b);
  FileName test;
  test = c;

  cout << prefix << "Testing Expanded [" << name << "]" << endl;

  cout << prefix << "\tName:              " << test.name() << endl;
  cout << prefix << "\tBase Name:         " << test.baseName() << endl;
  cout << prefix << "\tExtension:         " << test.extension() << endl;
  cout << prefix << "\tOriginal path      " << test.originalPath() << endl;
  cout << prefix << "\tExists             " << test.fileExists() << endl;

  cout << endl;
}