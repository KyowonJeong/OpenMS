// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
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
// $Maintainer: $
// $Authors: $
// --------------------------------------------------------------------------

#include <OpenMS/CONCEPT/ClassTest.h>
#include <OpenMS/test_config.h>

///////////////////////////
#include <OpenMS/MATH/MISC/BSpline2d.h>
#include <OpenMS/FORMAT/CsvFile.h>
///////////////////////////

using namespace OpenMS;
using namespace std;

START_TEST(BSpline2d, "$Id$")

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

BSpline2d* ptr = nullptr;
BSpline2d* null_ptr = nullptr;

/* load data generated by R code:
   # add noise on x and y=sin(x)
   s <- seq(-5,5,0.1)
   x <- s + rnorm(length(s))
   y <- 10 * sin(x) + rnorm(length(s))
   # duplicate x entries by subsampling length(x) rows with replacement
   df <- cbind(x,y)
   sub <- df[sample(length(x), replace=TRUE),]
   # add (again) noise to y values
   sub[,2] = sub[,2] + rnorm(length(sub[,2]))
   # append and order by x
   df <- rbind(df, sub)
   df <- df[order(df[,1]),]
*/
CsvFile sinus_file(OPENMS_GET_TEST_DATA_PATH("BSpline2d_test_sinus.txt"), '\t');
std::vector<double> x;
std::vector<double> y;
for (Size i = 0; i != sinus_file.rowCount(); ++i)
{
  StringList sl;
  if (sinus_file.getRow(i, sl))
  {
    x.push_back(sl[0].toDouble());
    y.push_back(sl[1].toDouble());
  }
}

START_SECTION((BSpline2d(const std::vector< double > &x, const std::vector< double > &y, double wave_length=0, BoundaryCondition boundary_condition=BC_ZERO_ENDPOINTS, Size num_nodes=0)))
{
  ptr = new BSpline2d(x, y);
  TEST_NOT_EQUAL(ptr, null_ptr)

  BSpline2d(x, y, 10, BSpline2d::BC_ZERO_ENDPOINTS);
  BSpline2d(x, y, 1, BSpline2d::BC_ZERO_FIRST);
  BSpline2d(x, y, 100, BSpline2d::BC_ZERO_SECOND);
}
END_SECTION

START_SECTION((virtual ~BSpline2d()))
{
  delete ptr;
}
END_SECTION

START_SECTION((bool solve(const std::vector< double > &y)))
{
  BSpline2d b(x, y);
  b.solve(y);
}
END_SECTION

START_SECTION((double eval(const double x) const))
{
  // calculate error of noisy points
  double mean_squared_error_noisy(0.0);
  for (Size i = 0; i != x.size(); ++i)
  {
    double error = y[i] - 10.0 * sin(x[i]);
    //cout << "Original Error: " << error << endl;
    mean_squared_error_noisy += error * error;
  }
  mean_squared_error_noisy /= (double)x.size();

  {
    // calculate error after smoothing
    BSpline2d b(x, y);
    double mean_squared_error_smoothed(0.0);
    for (Size i = 0; i != x.size(); ++i)
    {
      double error = b.eval(x[i]) - 10.0 * sin(x[i]);
      //cout << "Smoothed Error: " << error << endl;
      mean_squared_error_smoothed += error * error;
    }
    mean_squared_error_smoothed /= (double)x.size();

    // error of smoothed signal must be much lower (for this data it should be at least half of the unsmoothed one)
    TEST_EQUAL(mean_squared_error_smoothed < 0.5 * mean_squared_error_noisy, true)
  }

  {
    // calculate error after regularized smoothing
    BSpline2d b(x, y, 2.0);
    double mean_squared_error_smoothed(0.0);
    for (Size i = 0; i != x.size(); ++i)
    {
      double error = b.eval(x[i]) - 10.0 * sin(x[i]);
      mean_squared_error_smoothed += error * error;
    }
    mean_squared_error_smoothed /= (double)x.size();

    // error of smoothed signal must be lower (for this data it should be at least half of the unsmoothed one)
    TEST_EQUAL(mean_squared_error_smoothed < 0.5 * mean_squared_error_noisy, true)
  }
}
END_SECTION

START_SECTION((double derivative(const double x) const))
{
  {
    // calculate error on first derivative of smoothed points.
    // preserve curvature - otherwise we get large errors on derivative
    BSpline2d b(x, y, 0, BSpline2d::BC_ZERO_SECOND);
    double mean_absolute_derivative_error(0.0);
    for (Size i = 0; i != x.size(); ++i)
    {
      double error = fabs(b.derivative(x[i]) - 10.0 * cos(x[i]));
      mean_absolute_derivative_error += error;
    }
    mean_absolute_derivative_error /= (double)x.size();

    //cout << mean_absolute_derivative_error << endl;
    TEST_EQUAL(mean_absolute_derivative_error < 10.0 * 0.2, true)
  }

}
END_SECTION

START_SECTION((bool ok() const))
{
  vector<double> x;
  vector<double> y;
  for (Size i = 0; i != 10; ++i)
  {
    x.push_back(i);
    y.push_back(i);
  }
  BSpline2d b1(x, y, 0, BSpline2d::BC_ZERO_SECOND, 100);
  double y1 = b1.eval(5.5);
  TEST_EQUAL(b1.ok(), true);

  x.clear();
  y.clear();
  for (Size i = 10; i != 0; --i)
  {
    x.push_back(i);
    y.push_back(i);
  }
  BSpline2d b2(x, y, 0, BSpline2d::BC_ZERO_SECOND, 100);
  double y2 = b2.eval(5.5);
  TEST_EQUAL(b2.ok(), true);

  TEST_REAL_SIMILAR(y1,y2);
}
END_SECTION

START_SECTION((void debug(bool enable)))
{
  NOT_TESTABLE
}
END_SECTION

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
END_TEST



