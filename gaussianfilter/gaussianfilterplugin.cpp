/* gaussfilter.cxx
 * 2009-06-03: create this program by Yang Yu
 * 2009-08-14: change into plugin by Yang Yu
 */

// Adapted and upgraded to V3DPluginInterface2_1 by Jianlong Zhou, 2012-04-05
// add dofunc() by Jianlong Zhou, 2012-04-08

#include <QtGui>
#include <v3d_interface.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <QMessageBox>
#include <QDialog>
#include "v3d_message.h"
#include "stackutil.h"
#include "gaussianfilterplugin.h"

using namespace std;

#define INF 1E9

//Q_EXPORT_PLUGIN2 ( PluginName, ClassName )
//The value of PluginName should correspond to the TARGET specified in the plugin's project file.
//Q_EXPORT_PLUGIN2(gaussianfilter, GaussianFilterPlugin)

void processImage(V3DPluginCallback2 &callback, QWidget *parent);
bool processImage(V3DPluginCallback2 &callback, const V3DPluginArgList & input, V3DPluginArgList & output);
template <class T> void gaussian_filter(T* data1d,
                                        V3DLONG *in_sz,
                                        unsigned int Wx,
                                        unsigned int Wy,
                                        unsigned int Wz,
                                        unsigned int c,
                                        double sigma,
                                        float* &outimg);

const QString title = QObject::tr("Gaussian Filter Plugin");
QStringList GaussianFilterPlugin::menulist() const
{
    return QStringList() << tr("Gaussian Filter") << tr("About")<<tr("superplugin");
}

void GaussianFilterPlugin::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
	if (menu_name == tr("Gaussian Filter"))
	{
		processImage(callback,parent);
	}
	else if (menu_name == tr("About"))
	{
		v3d_msg("Gaussian filter.");
	}
    else if (menu_name == tr("superplugin"))
    {
        cout<<add(150,62,callback)<<endl;
    }
}

QStringList GaussianFilterPlugin::funclist() const
{
	return QStringList()
		<<tr("gf")
        <<tr("help")
        <<tr("superplugin");
}


bool GaussianFilterPlugin::dofunc(const QString &func_name, const V3DPluginArgList &input, V3DPluginArgList &output, V3DPluginCallback2 &callback, QWidget *parent)
{
     if (func_name == tr("gf"))
	{
        return processImage(callback, input, output);
	}
	else if(func_name == tr("help"))
	{
		cout<<"Usage : v3d -x gaussian -f gf -i <inimg_file> -o <outimg_file> -p <wx> <wy> <wz> <channel> <sigma>"<<endl;
		cout<<endl;
		cout<<"wx          filter window size (pixel #) in x direction, default 7 and maximum image xsize-1"<<endl;
		cout<<"wy          filter window size (pixel #) in y direction, default 7 and maximum image ysize-1"<<endl;
		cout<<"wz          filter window size (pixel #) in z direction, default 3 and maximum image zsize-1"<<endl;
		cout<<"channel     the input channel value, default 1 and start from 1"<<endl;
		cout<<"sigma       filter sigma, default 1.0"<<endl;
		cout<<endl;
		cout<<"e.g. v3d -x gaussian -f gf -i input.raw -o output.raw -p 3 3 3 1 1.0"<<endl;
		cout<<endl;
		return true;
	}
     else if (func_name == tr("superplugin"))
     {
         cout<<add(150,62,callback)<<endl;
         return true;
     }
}

bool processImage(V3DPluginCallback2 &callback, const V3DPluginArgList & input, V3DPluginArgList & output)
{
	cout<<"Welcome to Gaussian filter"<<endl;
    //if (output.size() >2) return false;

    int isout=1;
    int isdatafile=1;
    unsigned int Wx=7, Wy=7, Wz=7, c=1;
     float sigma = 3.0;
     if (input.size()>=2)
     {
          vector<char*> paras = (*(vector<char*> *)(input.at(1).p));
          if(paras.size() >= 1) isdatafile = atoi(paras.at(0));
          if(paras.size() >= 2) isout = atoi(paras.at(1));
          if(paras.size() >= 3) Wx = atoi(paras.at(2));
          if(paras.size() >= 4) Wy = atoi(paras.at(3));
          if(paras.size() >= 5) Wz = atoi(paras.at(4));
          if(paras.size() >= 6) c = atoi(paras.at(5));
          if(paras.size() >= 7) sigma = atof(paras.at(6));

	}


    Image4DSimple *inimg;
    char * inimg_file;
    if(isdatafile==1){
        inimg_file = ((vector<char*> *)(input.at(0).p))->at(0);
        inimg = callback.loadImage(inimg_file);
        cout<<"inimg_file = "<<inimg_file<<endl;
    }else{
        inimg=((vector<Image4DSimple*> *)(input.at(0).p))->at(0);
    }



    Image4DSimple *outputimg;
    char * outimg_file;
    if(isout==0){
        outputimg=((vector<Image4DSimple*> *)(output.at(0).p))->at(0);
        cout<<"outputimg: "<<outputimg<<endl;
    }else{
        outimg_file = ((vector<char*> *)(output.at(0).p))->at(0);
        outputimg=new Image4DSimple();
        cout<<"outimg_file = "<<outimg_file<<endl;
    }
    cout<<"isdatafile = "<<isdatafile<<endl;
    cout<<"isout = "<<isout<<endl;
	cout<<"Wx = "<<Wx<<endl;
     cout<<"Wy = "<<Wy<<endl;
	cout<<"Wz = "<<Wz<<endl;
     cout<<"c = "<<c<<endl;
     cout<<"sigma = "<<sigma<<endl;



     double sigma_s2 = 0.5/(sigma*sigma);


    if (!inimg || !inimg->valid())
    {
        v3d_msg("Fail to open the image file.", 0);
        return false;
    }

     if(c > inimg->getCDim())// check the input channel number range
     {
          v3d_msg("The input channel number is out of real channel range.\n", 0 );
          return false;
     }

	//input
     float* outimg = 0; //no need to delete it later as the Image4DSimple variable "outimg" will do the job

     V3DLONG in_sz[4];
     in_sz[0] = inimg->getXDim();
     in_sz[1] = inimg->getYDim();
     in_sz[2] = inimg->getZDim();
     in_sz[3] = inimg->getCDim();
    cout<<"ready to gf"<<endl;
     switch (inimg->getDatatype())
     {
          case V3D_UINT8: gaussian_filter(inimg->getRawData(), in_sz, Wx, Wy, Wz, c, sigma, outimg); break;
          case V3D_UINT16: gaussian_filter((unsigned short int*)(inimg->getRawData()), in_sz, Wx, Wy, Wz, c, sigma, outimg); break;
          case V3D_FLOAT32: gaussian_filter((float *)(inimg->getRawData()), in_sz, Wx, Wy, Wz, c, sigma, outimg); break;
          default:
               v3d_msg("Invalid datatype in Gaussian fileter.", 0);
               if (inimg) {delete inimg; inimg=0;}
               return false;
     }
    cout<<"gf done"<<endl;
     // save image
     //Image4DSimple outimg1;

    float max_v=0,min_v=255;
    unsigned char * pPost;
    V3DLONG imsz=in_sz[0]*in_sz[1]*in_sz[2]*in_sz[3];
    for(V3DLONG i=0; i<imsz; i++)
    {
        if(max_v<outimg[i]) max_v = outimg[i];
        if(min_v>outimg[i]) min_v = outimg[i];
    }
    max_v -= min_v;
    pPost=new unsigned char[imsz];
    if(max_v>0)
    {
        for(V3DLONG i=0; i<imsz; i++)
            pPost[i] = (unsigned char) 255*(double)(outimg[i] - min_v)/max_v;
    }
    else
    {
        for(V3DLONG i=0; i<imsz; i++)
            pPost[i] = (unsigned char) outimg[i];
    }


    cout<<"ready to set"<<endl;
     outputimg->setData((unsigned char *)pPost, in_sz[0], in_sz[1], in_sz[2], 1, V3D_UINT8);
     cout<<"set done"<<endl;
    if(isout){
         cout<<"ready tp save"<<endl;
         callback.saveImage(outputimg, outimg_file);
         cout<<"saved"<<endl;
    }
     if(inimg) {delete inimg; inimg =0;}

     return true;
}

void processImage(V3DPluginCallback2 &callback, QWidget *parent)
{
    v3dhandle curwin = callback.currentImageWindow();
    if (!curwin)
    {
        QMessageBox::information(0, "", "You don't have any image open in the main window.");
        return;
    }

    Image4DSimple* p4DImage = callback.getImage(curwin);

    if (!p4DImage)
    {
        QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
        return;
    }

    unsigned char* data1d = p4DImage->getRawData();
    //V3DLONG totalpxls = p4DImage->getTotalBytes();
    V3DLONG pagesz = p4DImage->getTotalUnitNumberPerChannel();

     V3DLONG N = p4DImage->getXDim();
     V3DLONG M = p4DImage->getYDim();
     V3DLONG P = p4DImage->getZDim();
     V3DLONG sc = p4DImage->getCDim();

     //add input dialog

    GaussianFilterDialog dialog(callback, parent);
    if (!dialog.image)
        return;

    if (dialog.exec()!=QDialog::Accepted)
        return;

    dialog.update();

    Image4DSimple* subject = dialog.image;
    if (!subject)
        return;
    ROIList pRoiList = dialog.pRoiList;

    int Wx = dialog.Wx;
    int Wy = dialog.Wy;
    int Wz = dialog.Wz;
    int c = dialog.ch;
    double sigma = dialog.sigma;

    cout<<"Wx = "<<Wx<<endl;
    cout<<"Wy = "<<Wy<<endl;
    cout<<"Wz = "<<Wz<<endl;
    cout<<"sigma = "<<sigma<<endl;
    cout<<"ch = "<<c<<endl;

    // gaussian_filter
     V3DLONG in_sz[4];
     in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

    float* outimg = 0;
    switch (p4DImage->getDatatype())
    {
        case V3D_UINT8: gaussian_filter(data1d, in_sz, Wx, Wy, Wz, c, sigma, outimg); break;
        case V3D_UINT16: gaussian_filter((unsigned short int *)data1d, in_sz, Wx, Wy, Wz, c, sigma, outimg); break;
        case V3D_FLOAT32: gaussian_filter((float *)data1d, in_sz, Wx, Wy, Wz, c, sigma, outimg);break;
        default: v3d_msg("Invalid data type. Do nothing."); return;
    }


     // display
     Image4DSimple * new4DImage = new Image4DSimple();
     new4DImage->setData((unsigned char *)outimg, N, M, P, 1, V3D_FLOAT32);
     //ljs,dlc,csz
     //给terafly cViewer 的imgdata

//    const Image4DSimple *newTerafly4dImage = callback.getImageTeraFly();

//    const unsigned char *temp = newTerafly4dImage->getRawData();//获取data1d


//    qDebug()<<"-------------------jazz";
//    qDebug()<<temp;
//    qDebug()<<"-------------------jazz";
    //把temp放进CViewer::setImageData里

//    temp = new4DImage->getRawData();
//    qDebug()<<temp;
    //callback.putDataToCViewer(temp,&callback);


     v3dhandle newwin = callback.newImageWindow();
     callback.setImage(newwin, new4DImage);
     callback.setImageName(newwin, title);

     callback.updateImageWindow(newwin);

     //callback.pushImageToTeraWin(newwin);




	return;
}



template <class T> void gaussian_filter(T* data1d,
                     V3DLONG *in_sz,
                     unsigned int Wx,
                     unsigned int Wy,
                     unsigned int Wz,
                     unsigned int c,
                     double sigma,
                     float* &outimg)
{
    if (!data1d || !in_sz || in_sz[0]<=0 || in_sz[1]<=0 || in_sz[2]<=0 || in_sz[3]<=0 || outimg)
    {
        v3d_msg("Invalid parameters to gaussian_filter().", 0);
        return;
    }
	
	if (outimg)
    {
        v3d_msg("Warning: you have supplied an non-empty output image pointer. This program will force to free it now. But you may want to double check.");
        delete []outimg;
        outimg = 0;
    }

     // for filter kernel
     double sigma_s2 = 0.5/(sigma*sigma); // 1/(2*sigma*sigma)
     double pi_sigma = 1.0/(sqrt(2*3.1415926)*sigma); // 1.0/(sqrt(2*pi)*sigma)

     float min_val = INF, max_val = 0;

     V3DLONG N = in_sz[0];
     V3DLONG M = in_sz[1];
     V3DLONG P = in_sz[2];
     V3DLONG sc = in_sz[3];
     V3DLONG pagesz = N*M*P;

     //filtering
     V3DLONG offset_init = (c-1)*pagesz;

     //declare temporary pointer
     float *pImage = new float [pagesz];
     if (!pImage)
     {
          printf("Fail to allocate memory.\n");
          return;
     }
     else
     {
          for(V3DLONG i=0; i<pagesz; i++)
               pImage[i] = data1d[i + offset_init];  //first channel data (red in V3D, green in ImageJ)
     }
       //Filtering
     //
     //   Filtering along x
     if(N<2)
     {
          //do nothing
     }
     else
     {
          //create Gaussian kernel
          float  *WeightsX = 0;
          WeightsX = new float [Wx];
          if (!WeightsX)
               return;

          float Half = (float)(Wx-1)/2.0;

          // Gaussian filter equation:
          // http://en.wikipedia.org/wiki/Gaussian_blur
       //   for (unsigned int Weight = 0; Weight < Half; ++Weight)
       //   {
       //        const float  x = Half* float (Weight) / float (Half);
      //         WeightsX[(int)Half - Weight] = WeightsX[(int)Half + Weight] = pi_sigma * exp(-x * x *sigma_s2); // Corresponding symmetric WeightsX
      //    }

          for (unsigned int Weight = 0; Weight <= Half; ++Weight)
          {
              const float  x = float(Weight)-Half;
              WeightsX[Weight] = WeightsX[Wx-Weight-1] = pi_sigma * exp(-(x * x *sigma_s2)); // Corresponding symmetric WeightsX
          }


          double k = 0.;
          for (unsigned int Weight = 0; Weight < Wx; ++Weight)
               k += WeightsX[Weight];

          for (unsigned int Weight = 0; Weight < Wx; ++Weight)
               WeightsX[Weight] /= k;

		 printf("\n x dierction");
		 
		 for (unsigned int Weight = 0; Weight < Wx; ++Weight)
			 printf("/n%f",WeightsX[Weight]);

          //   Allocate 1-D extension array
          float  *extension_bufferX = 0;
          extension_bufferX = new float [N + (Wx<<1)];

          unsigned int offset = Wx>>1;

          //	along x
          const float  *extStop = extension_bufferX + N + offset;
         
          for(V3DLONG iz = 0; iz < P; iz++)
          {
               for(V3DLONG iy = 0; iy < M; iy++)
               {
                    float  *extIter = extension_bufferX + Wx;
                    for(V3DLONG ix = 0; ix < N; ix++)
                    {
                         *(extIter++) = pImage[iz*M*N + iy*N + ix];
                    }

                    //   Extend image
                    const float  *const stop_line = extension_bufferX - 1;
                    float  *extLeft = extension_bufferX + Wx - 1;
                    const float  *arrLeft = extLeft + 2;
                    float  *extRight = extLeft + N + 1;
                    const float  *arrRight = extRight - 2;

                    while (extLeft > stop_line)
                    {
                         *(extLeft--) = *(arrLeft++);
                         *(extRight++) = *(arrRight--);
      
                    }

                    //	Filtering
                    extIter = extension_bufferX + offset;

                    float  *resIter = &(pImage[iz*M*N + iy*N]);

                    while (extIter < extStop)
                    {
                         double sum = 0.;
                         const float  *weightIter = WeightsX;
                         const float  *const End = WeightsX + Wx;
                         const float * arrIter = extIter;
                         while (weightIter < End)
                              sum += *(weightIter++) * float (*(arrIter++));
                         extIter++;
                         *(resIter++) = sum;

                         //for rescale
                         if(max_val<*arrIter) max_val = *arrIter;
                         if(min_val>*arrIter) min_val = *arrIter;
                  

                    }
       
               }
          }
          //de-alloc
           if (WeightsX) {delete []WeightsX; WeightsX=0;}
           if (extension_bufferX) {delete []extension_bufferX; extension_bufferX=0;}

     }

     //   Filtering along y
     if(M<2)
     {
          //do nothing
     }
     else
     {
          //create Gaussian kernel
          float  *WeightsY = 0;		  
          WeightsY = new float [Wy];
          if (!WeightsY)
               return;

          float Half = (float)(Wy-1)/2.0;

          // Gaussian filter equation:
          // http://en.wikipedia.org/wiki/Gaussian_blur
         /* for (unsigned int Weight = 0; Weight < Half; ++Weight)
          {
               const float  y = Half* float (Weight) / float (Half);
               WeightsY[(int)Half - Weight] = WeightsY[(int)Half + Weight] = pi_sigma * exp(-y * y *sigma_s2); // Corresponding symmetric WeightsY
          }*/

          for (unsigned int Weight = 0; Weight <= Half; ++Weight)
          {
              const float  y = float(Weight)-Half;
              WeightsY[Weight] = WeightsY[Wy-Weight-1] = pi_sigma * exp(-(y * y *sigma_s2)); // Corresponding symmetric WeightsY
          }


          double k = 0.;
          for (unsigned int Weight = 0; Weight < Wy; ++Weight)
               k += WeightsY[Weight];

          for (unsigned int Weight = 0; Weight < Wy; ++Weight)
               WeightsY[Weight] /= k;

          //	along y
          float  *extension_bufferY = 0;
          extension_bufferY = new float [M + (Wy<<1)];

          unsigned int offset = Wy>>1;
          const float *extStop = extension_bufferY + M + offset;

          for(V3DLONG iz = 0; iz < P; iz++)
          {
               for(V3DLONG ix = 0; ix < N; ix++)
               {
                    float  *extIter = extension_bufferY + Wy;
                    for(V3DLONG iy = 0; iy < M; iy++)
                    {
                         *(extIter++) = pImage[iz*M*N + iy*N + ix];
                    }

                    //   Extend image
                    const float  *const stop_line = extension_bufferY - 1;
                    float  *extLeft = extension_bufferY + Wy - 1;
                    const float  *arrLeft = extLeft + 2;
                    float  *extRight = extLeft + M + 1;
                    const float  *arrRight = extRight - 2;

                    while (extLeft > stop_line)
                    {
                         *(extLeft--) = *(arrLeft++);
                         *(extRight++) = *(arrRight--);
                    }

                    //	Filtering
                    extIter = extension_bufferY + offset;

                    float  *resIter = &(pImage[iz*M*N + ix]);

                    while (extIter < extStop)
                    {
                         double sum = 0.;
                         const float  *weightIter = WeightsY;
                         const float  *const End = WeightsY + Wy;
                         const float * arrIter = extIter;
                         while (weightIter < End)
                              sum += *(weightIter++) * float (*(arrIter++));
                         extIter++;
                         *resIter = sum;
                         resIter += N;

                         //for rescale
                         if(max_val<*arrIter) max_val = *arrIter;
                         if(min_val>*arrIter) min_val = *arrIter;

          
                    }
               
               }
          }

          //de-alloc
          if (WeightsY) {delete []WeightsY; WeightsY=0;}
          if (extension_bufferY) {delete []extension_bufferY; extension_bufferY=0;}


     }

     //  Filtering  along z
     if(P<2)
     {
          //do nothing
     }
     else
     {
          //create Gaussian kernel
          float  *WeightsZ = 0;
          WeightsZ = new float [Wz];
          if (!WeightsZ)
               return;

          float Half = (float)(Wz-1)/2.0;

         /* for (unsigned int Weight = 1; Weight < Half; ++Weight)
          {
               const float  z = Half * float (Weight) / Half;
               WeightsZ[(int)Half - Weight] = WeightsZ[(int)Half + Weight] = pi_sigma * exp(-z * z * sigma_s2) ; // Corresponding symmetric WeightsZ
          }*/

          for (unsigned int Weight = 0; Weight <= Half; ++Weight)
          {
              const float  z = float(Weight)-Half;
              WeightsZ[Weight] = WeightsZ[Wz-Weight-1] = pi_sigma * exp(-(z * z *sigma_s2)); // Corresponding symmetric WeightsZ
          }


          double k = 0.;
          for (unsigned int Weight = 0; Weight < Wz; ++Weight)
               k += WeightsZ[Weight];

          for (unsigned int Weight = 0; Weight < Wz; ++Weight)
               WeightsZ[Weight] /= k;

          //	along z
          float  *extension_bufferZ = 0;
          extension_bufferZ = new float [P + (Wz<<1)];

          unsigned int offset = Wz>>1;
          const float *extStop = extension_bufferZ + P + offset;

          for(V3DLONG iy = 0; iy < M; iy++)
          {
               for(V3DLONG ix = 0; ix < N; ix++)
               {

                    float  *extIter = extension_bufferZ + Wz;
                    for(V3DLONG iz = 0; iz < P; iz++)
                    {
                         *(extIter++) = pImage[iz*M*N + iy*N + ix];
                    }

                    //   Extend image
                    const float  *const stop_line = extension_bufferZ - 1;
                    float  *extLeft = extension_bufferZ + Wz - 1;
                    const float  *arrLeft = extLeft + 2;
                    float  *extRight = extLeft + P + 1;
                    const float  *arrRight = extRight - 2;

                    while (extLeft > stop_line)
                    {
                         *(extLeft--) = *(arrLeft++);
                         *(extRight++) = *(arrRight--);
                    }

                    //	Filtering
                    extIter = extension_bufferZ + offset;

                    float  *resIter = &(pImage[iy*N + ix]);

                    while (extIter < extStop)
                    {
                         double sum = 0.;
                         const float  *weightIter = WeightsZ;
                         const float  *const End = WeightsZ + Wz;
                         const float * arrIter = extIter;
                         while (weightIter < End)
                              sum += *(weightIter++) * float (*(arrIter++));
                         extIter++;
                         *resIter = sum;
                         resIter += M*N;

                         //for rescale
                         if(max_val<*arrIter) max_val = *arrIter;
                         if(min_val>*arrIter) min_val = *arrIter;

                    }
               
               }
          }

          //de-alloc
          if (WeightsZ) {delete []WeightsZ; WeightsZ=0;}
          if (extension_bufferZ) {delete []extension_bufferZ; extension_bufferZ=0;}


     }

    outimg = pImage;


    return;
}




int add(int a, int b, V3DPluginCallback2 &callback)
{
    int c=a+b;
    return c;
}
