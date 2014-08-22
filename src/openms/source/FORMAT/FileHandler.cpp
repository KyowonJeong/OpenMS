// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2013.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Stephan Aiche $
// $Authors: Marc Sturm $
// --------------------------------------------------------------------------

#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/TextFile.h>
#include <OpenMS/FORMAT/GzipIfstream.h>
#include <OpenMS/FORMAT/Bzip2Ifstream.h>

#include <QFile>
#include <QCryptographicHash>

#include <fstream>

using namespace std;

namespace OpenMS
{
  FileTypes::Type FileHandler::getType(const String& filename)
  {
    FileTypes::Type type = getTypeByFileName(filename);
    if (type == FileTypes::UNKNOWN)
    {
      type = getTypeByContent(filename);
    }
    return type;
  }

  FileTypes::Type FileHandler::getTypeByFileName(const String& filename)
  {
    String basename = File::basename(filename), tmp;
    // special rules for "double extensions":
    if (basename.hasSuffix(".pep.xml"))
      return FileTypes::PEPXML;

    if (basename.hasSuffix(".prot.xml"))
      return FileTypes::PROTXML;

    try
    {
      tmp = basename.suffix('.');
    }
    // no '.' => unknown type
    catch (Exception::ElementNotFound&)
    {
      // last chance, Bruker fid file
      if (basename == "fid")
      {
        return FileTypes::XMASS;
      }
      return FileTypes::UNKNOWN;
    }
    tmp.toUpper();
    if (tmp == "BZ2" || tmp == "GZ") // todo ZIP (not supported yet):       || tmp == "ZIP"
    {
      // do not use getTypeByContent() here, as this is deadly for output files!
      return getTypeByFileName(filename.prefix(filename.size() - tmp.size() - 1)); // check name without compression suffix (e.g. bla.mzML.gz --> bla.mzML)
    }

    return FileTypes::nameToType(tmp);
  }

  bool FileHandler::isSupported(FileTypes::Type type)
  {
    if (type == FileTypes::UNKNOWN || type == FileTypes::SIZE_OF_TYPE)
    {
      return false;
    }
    else
    {
      return true;
    }
  }

  FileTypes::Type FileHandler::getTypeByContent(const String& filename)
  {
    String first_line;
    String two_five;
    String all_simple;

    // only the first five lines will be set for compressed files
    // so far, compression is only supported for XML files
    vector<String> complete_file;

    // test whether the file is compressed (bzip2 or gzip)
    ifstream compressed_file(filename.c_str());
    char bz[2];
    compressed_file.read(bz, 2);
    char g1 = 0x1f;
    char g2 = 0;
    g2 |= 1 << 7;
    g2 |= 1 << 3;
    g2 |= 1 << 1;
    g2 |= 1 << 0;
    compressed_file.close();
    if (bz[0] == 'B' && bz[1] == 'Z') // bzip2
    {
      Bzip2Ifstream bzip2_file(filename.c_str());
      char buffer[1024];
      bzip2_file.read(buffer, 1024);
      String buffer_str(buffer);
      vector<String> split;
      buffer_str.split('\n', split);
      split.resize(5);
      first_line = split[0];
      two_five = split[1] + ' ' + split[2] + ' ' + split[3] + ' ' + split[4];
      all_simple = first_line + ' ' + two_five;
      complete_file = split;
    }
    else if (bz[0] == g1 && bz[1] == g2) // gzip
    {
      GzipIfstream gzip_file(filename.c_str());
      char buffer[1024];
      gzip_file.read(buffer, 1024);
      String buffer_str(buffer);
      vector<String> split;
      buffer_str.split('\n', split);
      split.resize(5);
      first_line = split[0];
      two_five = split[1] + ' ' + split[2] + ' ' + split[3] + ' ' + split[4];
      all_simple = first_line + ' ' + two_five;
      complete_file = split;
    }
    //else {} // TODO: ZIP
    else // uncompressed
    {
      //load first 5 lines
      TextFile file(filename, true, 5);
      file.resize(5); // in case not enough lines are in the file
      two_five = file[1] + ' ' + file[2] + ' ' + file[3] + ' ' + file[4];
      two_five.substitute('\t', ' ');
      all_simple = file[0] + ' ' + two_five;
      first_line = file[0];
      complete_file = file;
    }
    //std::cerr << "\n Line1:\n" << first_line << "\nLine2-5:\n" << two_five << "\nall:\n" << all_simple << "\n\n";


    //mzXML (all lines)
    if (all_simple.hasSubstring("<mzXML"))
      return FileTypes::MZXML;

    //mzData (all lines)
    if (all_simple.hasSubstring("<mzData"))
      return FileTypes::MZDATA;

    //mzML (all lines)
    if (all_simple.hasSubstring("<mzML"))
      return FileTypes::MZML;

    //"analysisXML" aka. mzid (all lines)
    if (all_simple.hasSubstring("<mzIdentML"))
      return FileTypes::MZIDENTML;

    //mzq (all lines)
    if (all_simple.hasSubstring("<qcML"))
      return FileTypes::MZQUANTML;

    //subject to change!
    if (all_simple.hasSubstring("<MzQualityMLType"))
      return FileTypes::QCML;

    //pepXML (all lines)
    if (all_simple.hasSubstring("xmlns=\"http://regis-web.systemsbiology.net/pepXML\""))
      return FileTypes::PEPXML;

    //protXML (all lines)
    if (all_simple.hasSubstring("xmlns=\"http://regis-web.systemsbiology.net/protXML\""))
      return FileTypes::PROTXML;

    //feature map (all lines)
    if (all_simple.hasSubstring("<featureMap"))
      return FileTypes::FEATUREXML;

    //idXML (all lines)
    if (all_simple.hasSubstring("<IdXML"))
      return FileTypes::IDXML;

    //consensusXML (all lines)
    if (all_simple.hasSubstring("<consensusXML"))
      return FileTypes::CONSENSUSXML;

    //TOPPAS (all lines)
    if (all_simple.hasSubstring("<PARAMETERS") && all_simple.hasSubstring("<NODE name=\"info\"") && all_simple.hasSubstring("<ITEM name=\"num_vertices\""))
      return FileTypes::TOPPAS;

    //INI (all lines) (must be AFTER TOPPAS) - as this is less restrictive
    if (all_simple.hasSubstring("<PARAMETERS"))
      return FileTypes::INI;

    //TrafoXML (all lines)
    if (all_simple.hasSubstring("<TrafoXML"))
      return FileTypes::TRANSFORMATIONXML;

    //GelML (all lines)
    if (all_simple.hasSubstring("<GelML"))
      return FileTypes::GELML;

    //traML (all lines)
    if (all_simple.hasSubstring("<TraML"))
      return FileTypes::TRAML;

    //OMSSAXML file
    if (all_simple.hasSubstring("<MSResponse"))
      return FileTypes::OMSSAXML;

    //MASCOTXML file
    if (all_simple.hasSubstring("<mascot_search_results"))
      return FileTypes::MASCOTXML;

    //FASTA file
    // .. check this fairly early on, because other file formats might be less specific
    {
      Size i = 0;
      Size bigger_than = 0;
      while (i < complete_file.size())
      {
        if (complete_file[i].trim().hasPrefix(">"))
        {
          ++bigger_than;
          ++i;
        }
        else if (complete_file[i].trim().hasPrefix("#"))
          ++i;
        else
          break;
      }
      if (bigger_than > 0)
        return FileTypes::FASTA;
    }

    // PNG file (to be really correct, the first eight bytes of the file would
    // have to be checked; see e.g. the wikipedia article)
    if (first_line.substr(1, 3) == "PNG")
      return FileTypes::PNG;

    //MSP (all lines)
    for (Size i = 0; i != complete_file.size(); ++i)
    {
      if (complete_file[i].hasPrefix("Name: ") && complete_file[i].hasSubstring("/"))
      {
        return FileTypes::MSP;
      }
      if (complete_file[i].hasPrefix("Num peaks: "))
      {
        return FileTypes::MSP;
      }
    }

    //tokenize lines 2-5
    vector<String> parts;
    two_five.split(' ', parts);

    //DTA
    if (parts.size() == 8)
    {
      bool conversion_error = false;
      try
      {
        for (Size i = 0; i < 8; ++i)
        {
          parts[i].toFloat();
        }
      }
      catch (Exception::ConversionError)
      {
        conversion_error = true;
      }
      if (!conversion_error)
        return FileTypes::DTA;
    }

    //DTA2D
    if (parts.size() == 12)
    {
      bool conversion_error = false;
      try
      {
        for (Size i = 0; i < 12; ++i)
        {
          parts[i].toFloat();
        }
      }
      catch (Exception::ConversionError)
      {
        conversion_error = true;
      }
      if (!conversion_error)
        return FileTypes::DTA2D;
    }

    // MGF (Mascot Generic Format)
    if (two_five.hasSubstring("BEGIN IONS"))
    {
      return FileTypes::MGF;
    }
    else
    {
      for (Size i = 0; i != complete_file.size(); ++i)
      {
        if (complete_file[i].trim()=="FORMAT=Mascot generic" || complete_file[i].trim()=="BEGIN IONS")
        {
          return FileTypes::MGF;
        }
      }
    }

    // MS2 file format
    if (all_simple.hasSubstring("CreationDate"))
    {
      if (all_simple.size() > 0 && all_simple[0] == 'H')
      {
        return FileTypes::MS2;
      }
    }

    // msInspect file (.tsv)
    for (Size i = 0; i != complete_file.size(); ++i)
    {
      if (complete_file[i].hasSubstring("scan\ttime\tmz\taccurateMZ\tmass\tintensity\tcharge\tchargeStates\tkl\tbackground\tmedian\tpeaks\tscanFirst\tscanLast\tscanCount\ttotalIntensity\tsumSquaresDist\tdescription"))
      {
        return FileTypes::TSV;
      }
    }

    // specArray file (.pepList)
    if (first_line.hasSubstring("       m/z\t     rt(min)\t       snr\t      charge\t   intensity"))
    {
      return FileTypes::PEPLIST;
    }

    // hardkloer file (.hardkloer)
    /**
NOT IMPLEMENTED YET
if (first_line.hasSubstring("File	First Scan	Last Scan	Num of Scans	Charge	Monoisotopic Mass	Base Isotope Peak	Best Intensity	Summed Intensity	First RTime	Last RTime	Best RTime	Best Correlation	Modifications"))
    {
        return FileTypes::HARDKLOER;
    }
**/

    // kroenik file (.kroenik)
    if (first_line.hasSubstring("File\tFirst Scan\tLast Scan\tNum of Scans\tCharge\tMonoisotopic Mass\tBase Isotope Peak\tBest Intensity\tSummed Intensity\tFirst RTime\tLast RTime\tBest RTime\tBest Correlation\tModifications"))
    {
      return FileTypes::KROENIK;
    }

    // EDTA file
    // hard to tell... so we don't even try...

    return FileTypes::UNKNOWN;
  }

  PeakFileOptions& FileHandler::getOptions()
  {
    return options_;
  }

  const PeakFileOptions& FileHandler::getOptions() const
  {
    return options_;
  }

  void FileHandler::setOptions(const PeakFileOptions & options)
  {
      options_ = options;
  }

  String FileHandler::computeFileHash_(const String& filename) const
  {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    QFile file(filename.toQString());
    file.open(QFile::ReadOnly);
    while (!file.atEnd())
    {
      crypto.addData(file.read(8192));
    }
    return String((QString)crypto.result().toHex());
  }

} // namespace OpenMS