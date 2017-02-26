/*************************************************************************
This file is part of Logical Vision 2.

Logical Vision 2 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Logical Vision 2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Logical Vision 2.  If not, see <http://www.gnu.org/licenses/>.
************************************************************************/
#ifndef SUPER_PIXEL_HPP
#define SUPER_PIXEL_HPP

#include <array>
#include <armadillo>
#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>
#include <string>   // for strings

#include "../memread.hpp"

using namespace cv;
using namespace cv::ximgproc;
using namespace std;

/**************** SuperPixels Class *****************/
class SuperPixels {
public:
    // constructor
    SuperPixels(Mat *img, int alg = 0, int region_size = 10,
                float ruler = 10.0, int num_iter = 5,
                int min_element_size = 25);
    ~SuperPixels();

    // get amount of superpixels
    int getNumberOfSuperpixels() const;
    int getWidth() const;
    int getHeight() const;
    
    // get image with labels
    void getLabels(OutputArray labels_out) const;

    // get mask image with contour
    void getLabelContourMask(OutputArray image, bool thick_line = true) const;

private:
    int m_width;
    int m_height;
    
    // labels no
    int m_numlabels;
    
    // labels storage
    Mat m_klabels;

    // superpixels
    vector<Point2i> sp_centers;
    vector<vector<Point2i>> sp_ranges;
    arma::sp_umat sp_adj_mat;

    // private functions
    void init_superpixels();
};

/**************** Declaration *****************/
// create/display SuperpixelSLIC from input image
void show_super_pixels(Mat* img, SuperPixels *sp);

/**************** Implementation ***************/
SuperPixels::SuperPixels(Mat *img, int alg, int region_size,
                         float ruler, int num_iter,
                         int min_element_size) {
    Mat image = img->clone();
    GaussianBlur(image, image, Size(3, 3), 1, 1);
    Ptr<SuperpixelSLIC> slic = createSuperpixelSLIC(image, alg + SLIC, region_size, float(ruler));
    slic->iterate(num_iter);
    if (min_element_size > 0)
        slic->enforceLabelConnectivity(min_element_size);
    m_numlabels = slic->getNumberOfSuperpixels();
    slic->getLabels(m_klabels);
    m_width = img->size().width;
    m_height = img->size().height;

    // initialize superpixels
    init_superpixels();
}

SuperPixels::~SuperPixels() {}

int SuperPixels::getNumberOfSuperpixels() const {
    return m_numlabels;
}

int SuperPixels::getWidth() const {
    return m_width;
}

int SuperPixels::getHeight() const {
    return m_height;
}

void SuperPixels::getLabels(OutputArray labels_out) const {
    labels_out.assign(m_klabels);
}

void SuperPixels::getLabelContourMask(OutputArray _mask, bool _thick_line) const
{
    // default width
    int line_width = 2;

    if ( !_thick_line ) line_width = 1;

    _mask.create( m_height, m_width, CV_8UC1 );
    Mat mask = _mask.getMat();

    mask.setTo(0);

    const int dx8[8] = { -1, -1,  0,  1, 1, 1, 0, -1 };
    const int dy8[8] = {  0, -1, -1, -1, 0, 1, 1,  1 };

    int sz = m_width*m_height;

    vector<bool> istaken(sz, false);

    int mainindex = 0;
    for( int j = 0; j < m_height; j++ )
    {
      for( int k = 0; k < m_width; k++ )
      {
        int np = 0;
        for( int i = 0; i < 8; i++ )
        {
          int x = k + dx8[i];
          int y = j + dy8[i];

          if( (x >= 0 && x < m_width) && (y >= 0 && y < m_height) )
          {
            int index = y*m_width + x;

            if( false == istaken[index] )
            {
              if( m_klabels.at<int>(j,k) != m_klabels.at<int>(y,x) ) np++;
            }
          }
        }
        if( np > line_width )
        {
           mask.at<char>(j,k) = (uchar)255;
           istaken[mainindex] = true;
        }
        mainindex++;
      }
    }
}

void SuperPixels::init_superpixels() {
    // pixels in superpixels
    sp_ranges = vector<vector<Point2i>>(m_numlabels);
    for (int i = 0; i < m_height; i++) {
        for (int j = 0; j < m_width; j++) {
            sp_ranges[m_klabels.at<int>(i, j)].push_back(Point2i(j, i));
        }

    }
    
    // center of superpixel
    sp_centers = vector<Point2i>(m_numlabels);    
    for (auto i = 0; i < m_numlabels; i++) {
        int x = 0, y = 0;
        for (unsigned j = 0; j < sp_ranges[i].size(); j++) {
            x += sp_ranges[i][j].x;
            y += sp_ranges[i][j].y;
        }
        x = x/sp_ranges[i].size();
        y = y/sp_ranges[i].size();        
        sp_centers[i] = Point2i(x, y);
    }

    // adjacency mat
    sp_adj_mat = arma::sp_umat(m_numlabels, m_numlabels);
    const int dx8[8] = {-1, -1,  0,  1, 1, 1, 0, -1};
    const int dy8[8] = { 0, -1, -1, -1, 0, 1, 1,  1};

    int sz = m_width*m_height;

    vector<bool> istaken(sz, false);

    for (int j = 0; j < m_height; j++) {
        for (int k = 0; k < m_width; k++) {
            for (int i = 0; i < 8; i++) {
                int x = k + dx8[i];
                int y = j + dy8[i];
                if ((x >= 0 && x < m_width) && (y >= 0 && y < m_height)) {
                    int index = y*m_width + x;
                    if (false == istaken[index]) {
                        arma::uword a = m_klabels.at<int>(j,k);
                        arma::uword b = m_klabels.at<int>(y,x);
                        if ((a != b) && sp_adj_mat(a, b) == 0) {
                            sp_adj_mat(a, b) = 1;
                            sp_adj_mat(b, a) = 1;
                        }
                    }
                }
            }
        }
    }
}

void show_super_pixels(Mat* img, SuperPixels* slic) {
    Mat frame = img->clone();
    cvtColor(frame, frame, COLOR_Lab2BGR);
    Mat mask;
    slic->getLabelContourMask(mask, true);
    frame.setTo(Scalar(0, 0, 255), mask);
    imshow("Superpixel SLIC", frame);
    waitKey(0);
    destroyWindow("Superpixel SLIC");
}

#endif