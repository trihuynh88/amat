/*
* Copyright (C) 2014  Fernando Amat
* See license.txt for full license and copyright notice.
*
* Authors: Fernando Amat
*  mainTest_klnIO.cxx
*
*  Created on: October 1st, 2014
*      Author: Fernando Amat
*
* \brief Test the main klb I/O library
*
*
*/


#include <string>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>

#include "klb_imageIO.h"


using namespace std;
typedef std::chrono::high_resolution_clock Clock;

int main(int argc, const char** argv)
{
	int numThreads = -1;//<= 0 indicates use as many as possible
	KLB_COMPRESSION_TYPE compressionType = KLB_COMPRESSION_TYPE::BZIP2;//1->bzip2; 0->none
	std::string filenameOut;//("test000.klb");
	
	size_t pos = 0;
	ifstream inheader;//("000.nhdr");
	ifstream infile;
	string oneline;
	string token;
	int imdim;
	unsigned int imsize[4];
	string type;

	if (argc==3)
	{
		filenameOut.assign(argv[2]);
		char tmpname[200];
		sprintf(tmpname,"%s.nhdr",argv[1]);
		inheader.open(tmpname);
		sprintf(tmpname,"%s.raw",argv[1]);
		infile.open(tmpname,ios::in|ios::binary);
	}
	else
	{	
		cout<<"Need exactly 2 filename arguments: ./prog infile outfile\n";
		return 0;
	}
	while (inheader)
	{
		getline(inheader, oneline);
		string delim = ": ";
		if ((pos = oneline.find(delim)) != string::npos)
		{
			token = oneline.substr(0,pos);
			oneline.erase(0, pos+delim.length());
			if (token == "type")
			{
				type = oneline;
			}
			else if (token == "dimension")
			{
				imdim = atoi(oneline.c_str());			
			}
			else if (token == "sizes")
			{
				istringstream iss(oneline);
				for (int i=0; i<imdim; i++)
				{					
					string sub;
					iss>>sub;
					imsize[i] = atoi(sub.c_str());
				}
			}
			else
			{

			}
		}
	}

	cout<<"type is: "<<type<<endl;
	cout<<"dimension is: "<<imdim<<endl;
	cout<<"sizes are: ";
	for (int i=0; i<imdim; i++)
		cout<<imsize[i]<<" ";
	cout<<endl;
	/*
	if (imdim > 3)
	{
		cout<<"This program is currently supporting 3D data, the behavior in higher dimension is unknown. Quitting program."<<endl;
		return 1;
	}
	*/
	unsigned int klbsize[3];
	if (imdim == 3)
	{
		klbsize[0] = imsize[0];
		klbsize[1] = imsize[1];
		klbsize[2] = imsize[2];
	}
	else if (imdim == 4 && imsize[0] == 2)
	{
		klbsize[0] = imsize[1];
		klbsize[1] = imsize[2];
		klbsize[2] = imsize[3];
	}
	else
	{
		cout<<"The program currently supports either data of dimension 3 or data of dimension 4 with the first dimension corresponds to 2 channels (GFP, RFP).\n";
		return 1;
	}
	std::uint32_t	xyzct[KLB_DATA_DIMS] = {klbsize[0], klbsize[1], klbsize[2], 1, 1};
	std::uint32_t	blockSize[KLB_DATA_DIMS] = { 196, 96, 8, 1, 1 };


	//================================================
	//common definitions
	int err;
	//=========================================================================
	
	
	//initialize I/O object
	klb_imageIO imgIO( filenameOut );

	//setup header
	float32_t pixelSize_[KLB_DATA_DIMS];
	for (int ii = 0; ii < KLB_DATA_DIMS; ii++)
		pixelSize_[ii] = 1.0;

	char metadata_[KLB_METADATA_SIZE];
	sprintf(metadata_, "Testing metadata");
	KLB_DATA_TYPE datatype;
	unsigned int datatypesize;
	if (type=="short")
	{
		datatype = KLB_DATA_TYPE::INT16_TYPE;
		datatypesize = sizeof(short);
	}
	else if (type=="float")
	{
		datatype = KLB_DATA_TYPE::FLOAT32_TYPE;
		datatypesize = sizeof(float);
	}
	else
	{
		cout<<"Currently only support datatype of short and float. Quitting program."<<endl;
		return 1;
	}

	memcpy(imgIO.header.xyzct, xyzct, sizeof(uint32_t)* KLB_DATA_DIMS);
	memcpy(imgIO.header.blockSize, blockSize, sizeof(uint32_t)* KLB_DATA_DIMS);
	imgIO.header.setHeader(xyzct, datatype, pixelSize_, blockSize, compressionType, metadata_);
	imgIO.header.dataType = datatype;
	imgIO.header.compressionType = compressionType;
	
	unsigned int sizeorig=1;
	for (int i=0; i<imdim; i++)
		sizeorig *= imsize[i];
	char *img, *imgorig;
	if (type == "short")
	{
		img = (char*)(new int16_t[imgIO.header.getImageSizePixels()]);
		imgorig = (char*)(new int16_t[sizeorig]);
	}
	else
	{
		img = (char*)(new float[imgIO.header.getImageSizePixels()]);
		imgorig = (char*)(new float[sizeorig]);
	}
		

	//read raw data from file
	if (infile.is_open())
	{
		infile.read((char*)imgorig,sizeorig*datatypesize);
//imgIO.header.getImageSizePixels()*datatypesize);
		infile.close();
	}

	if (imdim==3)
	{
		memcpy(img,imgorig,imgIO.header.getImageSizePixels()*datatypesize);
	}
	else if (imdim==4 && imsize[0]==2)
	{
		if (type == "short")
		{
			short *imgs = (short*)img;
			short *imgorigs = (short*)imgorig;
			for (int i=0; i<imgIO.header.getImageSizePixels(); i++)
				imgs[i] = imgorigs[i*2];
		}
		else
		{
			float *imgf = (float*)img;
			float *imgorigf = (float*)imgorig;
			for (int i=0; i<imgIO.header.getImageSizePixels(); i++)
				imgf[i] = imgorigf[i*2];
		}
	}
	delete imgorig;

	cout << "Compressing file to " << filenameOut << endl;

	for (int aa = 0; aa < 1; aa++)
	{
		err = imgIO.writeImage((char*)img, numThreads);//all the threads available
		if (err > 0)
			return 2;
	}
	cout<<"Finished"<<endl;
	
	delete[] img;

	return 0;
}
