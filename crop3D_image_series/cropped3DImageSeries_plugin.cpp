/* cropped3DImageSeries_plugin.cpp
 * This is a test plugin, you can use it as a demo.
 * 2014-06-30 : by Zhi Zhou
 */
 
#include "v3d_message.h"
#include <vector>
#include "cropped3DImageSeries_plugin.h"
#include "../istitch/y_imglib.h"
#include "../../../hackathon/zhi/APP2_large_scale/readRawfile_func.h"
#include <qfile.h>
#include <qtextstream.h>
//#include "dirent.h"
#include <qdir.h>
//#include <fstream>
#include "basic_surf_objs.h"



using namespace std;
QStringList importSeriesFileList_addnumbersort(const QString & curFilePath)
{
    QStringList myList;
    myList.clear();

    // get the image files namelist in the directory
    QStringList imgSuffix;
    imgSuffix<<"*.tif"<<"*.raw"<<"*.v3draw"<<"*.lsm"
            <<"*.TIF"<<"*.RAW"<<"*.V3DRAW"<<"*.LSM";

    QDir dir(curFilePath);
    if (!dir.exists())
    {
        qWarning("Cannot find the directory");
        return myList;
    }

    foreach (QString file, dir.entryList(imgSuffix, QDir::Files, QDir::Name))
    {
        myList += QFileInfo(dir, file).absoluteFilePath();
    }

    // print filenames
    foreach (QString qs, myList)  qDebug() << qs;

    return myList;
}

template <class T> bool extract_a_channel(T* data1d, V3DLONG *sz, V3DLONG c, void * &outimg);


//Q_EXPORT_PLUGIN2(cropped3DImageSeries, cropped3DImageSeries);

 
QStringList cropped3DImageSeries::menulist() const
{
	return QStringList() 
        <<tr("crop a 3D stack from image series")
        <<tr("crop a 3D stack from a 3D image(v3draw/raw format)")
        <<tr("crop a 3D stack from a 3D image(terafly format)")
        <<tr("about");
}

QStringList cropped3DImageSeries::funclist() const
{
	return QStringList()
		<<tr("crop3DImageSeries")
		<<tr("help")
		<<tr("cropTeraFly")
		<< tr("teraflyCompine");
}

void cropped3DImageSeries::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
    if (menu_name == tr("crop a 3D stack from image series"))
    {
        QString m_InputfolderName = QFileDialog::getExistingDirectory(parent, QObject::tr("Choose the directory including all images "),
                                              QDir::currentPath(),
                                              QFileDialog::ShowDirsOnly);

        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();

        unsigned char * data1d_1st = 0;
        V3DLONG in_sz[4];
        int datatype;

        if (!simple_loadimage_wrapper(callback,const_cast<char *>(vim.tilesList.at(0).fn_image.c_str()), data1d_1st, in_sz, datatype))
        {
            fprintf (stderr, "Error happens in reading the subject file [%0]. Exit. \n",vim.tilesList.at(0).fn_image.c_str());
            return;
        }
        if(data1d_1st) {delete []data1d_1st; data1d_1st = 0;}

        V3DLONG sz[4];
        sz[0] = in_sz[0];
        sz[1] = in_sz[1];
        sz[2] = NTILES;
        sz[3] = in_sz[3];

        CropRegionNavigateDialog dialog(parent, sz);
        if (dialog.exec()!=QDialog::Accepted)
            return;

        dialog.update();

        V3DLONG im_cropped_sz[4];
        im_cropped_sz[0] = dialog.xe - dialog.xs + 1;
        im_cropped_sz[1] = dialog.ye - dialog.ys + 1;
        im_cropped_sz[2] = dialog.ze - dialog.zs + 1;
        if(dialog.c == -1)
            im_cropped_sz[3] = in_sz[3];
        else
            im_cropped_sz[3] = 1;

        unsigned char *im_cropped = 0;
        V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
        try {im_cropped = new unsigned char [pagesz];}
        catch(...)  {v3d_msg("cannot allocate memory for image_mip."); return;}

        V3DLONG i = 0;
        for(V3DLONG ii = dialog.zs; ii <= dialog.ze; ii++)
        {
            unsigned char * data1d = 0;
            V3DLONG in_sz[4];
            int datatype;

            if (!simple_loadimage_wrapper(callback,const_cast<char *>(vim.tilesList.at(ii).fn_image.c_str()), data1d, in_sz, datatype))
            {
                fprintf (stderr, "Error happens in reading the subject file [%0]. Exit. \n",vim.tilesList.at(ii).fn_image.c_str());
                return;
            }
            V3DLONG offsetc;
            if(dialog.c != -1)
            {
                offsetc = dialog.c * in_sz[0] *in_sz[1];
                for(V3DLONG iy = dialog.ys; iy <= dialog.ye; iy++)
                {
                    V3DLONG offsetj = iy*in_sz[0];
                    for(V3DLONG ix = dialog.xs; ix <= dialog.xe; ix++)
                    {
                        im_cropped[i] = data1d[offsetc + offsetj + ix];
                        i++;
                    }
                }
            }
            else
            {
                V3DLONG offsetk_cropped = (ii - dialog.zs) *im_cropped_sz[0] * im_cropped_sz[1];
                for(V3DLONG ic = 0; ic < in_sz[3]; ic++)
                {
                    offsetc = ic * in_sz[0] *in_sz[1] * in_sz[2];
                    V3DLONG offsetc_cropped = ic * im_cropped_sz[0] * im_cropped_sz[1] * im_cropped_sz[2];
                    for(V3DLONG iy = dialog.ys; iy <= dialog.ye; iy++)
                    {
                        V3DLONG offsetj = iy*in_sz[0];
                        V3DLONG offsetj_cropped = (iy - dialog.ys) *im_cropped_sz[0];
                        for(V3DLONG ix = dialog.xs; ix <= dialog.xe; ix++)
                        {
                            im_cropped[offsetc_cropped + offsetk_cropped + offsetj_cropped + ix - dialog.xs] = data1d[offsetc + offsetj + ix];
                        }
                    }
                }
            }

            if(data1d) {delete []data1d; data1d = 0;}
        }

        ImagePixelType pixeltype;
        switch (datatype)
        {
            case 1: pixeltype = V3D_UINT8; break;
            case 2: pixeltype = V3D_UINT16; break;
            case 4: pixeltype = V3D_FLOAT32;break;
            default: v3d_msg("Invalid data type. Do nothing."); return;
        }

        Image4DSimple * new4DImage = new Image4DSimple();
        new4DImage->setData((unsigned char *)im_cropped, im_cropped_sz[0], im_cropped_sz[1], im_cropped_sz[2], im_cropped_sz[3], pixeltype);
        v3dhandle newwin = callback.newImageWindow();
        callback.setImage(newwin, new4DImage);
        callback.setImageName(newwin, "Cropped 3D image");
        callback.updateImageWindow(newwin);

	}
    else if (menu_name == tr("crop a 3D stack from a 3D image(v3draw/raw format)"))
    {
        QString m_InputfolderName =  QFileDialog::getOpenFileName(0, QObject::tr("Open Raw File"),
                                                                  "",
                                                                  QObject::tr("Supported file (*.raw *.RAW *.V3DRAW *.v3draw)"));
        unsigned char * datald = 0;
        V3DLONG *in_zz = 0;
        V3DLONG *in_sz = 0;

        int datatype;

        if (!loadRawRegion(const_cast<char *>(m_InputfolderName.toStdString().c_str()), datald, in_zz, in_sz,datatype,0,0,0,1,1,1))
        {
            return;
        }

        if(datald) {delete []datald; datald = 0;}
        CropRegionNavigateDialog dialog(parent, in_zz);
        if (dialog.exec()!=QDialog::Accepted)
            return;
        dialog.update();

        unsigned char * cropped_image = 0;

        if (!loadRawRegion(const_cast<char *>(m_InputfolderName.toStdString().c_str()), cropped_image, in_zz, in_sz,datatype,dialog.xs,dialog.ys,dialog.zs,dialog.xe+1,dialog.ye+1,dialog.ze+1))
        {
            return;
        }

        ImagePixelType pixeltype;
        switch (datatype)
        {
            case 1: pixeltype = V3D_UINT8; break;
            case 2: pixeltype = V3D_UINT16; break;
            case 4: pixeltype = V3D_FLOAT32;break;
            default: v3d_msg("Invalid data type. Do nothing."); return;
        }

        Image4DSimple * new4DImage = new Image4DSimple();

        void* cropped_image_1channel = 0;
        if(dialog.c !=-1 && in_sz[3] > 1)
        {
            switch(datatype)
            {
                case 1: extract_a_channel(cropped_image, in_sz, dialog.c, cropped_image_1channel); break;
                case 2: extract_a_channel((unsigned short int *)cropped_image, in_sz, dialog.c, cropped_image_1channel); break;
                case 4: extract_a_channel((float *)cropped_image, in_sz, dialog.c, cropped_image_1channel); break;
                default:
                v3d_msg("Right now this plugin supports only UINT8/UINT16/FLOAT32 data. Do nothing."); return;
            }
            new4DImage->createImage(in_sz[0], in_sz[1], in_sz[2], 1, pixeltype);
            memcpy(new4DImage->getRawData(), (unsigned char *)cropped_image_1channel, new4DImage->getTotalBytes());
        }
        else
        {
            new4DImage->createImage(in_sz[0], in_sz[1], in_sz[2], in_sz[3], pixeltype);
            memcpy(new4DImage->getRawData(), (unsigned char *)cropped_image, new4DImage->getTotalBytes());
        }

        v3dhandle newwin = callback.newImageWindow();
        callback.setImage(newwin, new4DImage);
        callback.setImageName(newwin, "Cropped 3D image");
        callback.updateImageWindow(newwin);
        return;
    }
    else if (menu_name == tr("crop a 3D stack from a 3D image(terafly format)"))
    {
        QString m_InputfolderName = QFileDialog::getExistingDirectory(parent, QObject::tr("Choose the directory including the highest terafly images "),
                                                                      QDir::currentPath(),
                                                                      QFileDialog::ShowDirsOnly);
        V3DLONG *in_zz;
        if(!callback.getDimTeraFly(m_InputfolderName.toStdString(),in_zz))
        {
            v3d_msg("Cannot load terafly images.");
            return;
        }

        CropRegionNavigateDialog dialog(parent, in_zz);
        if (dialog.exec()!=QDialog::Accepted)
            return;
        dialog.update();

        unsigned char * cropped_image = 0;
        cropped_image = callback.getSubVolumeTeraFly(m_InputfolderName.toStdString(),dialog.xs,dialog.xe+1,dialog.ys,dialog.ye+1,dialog.zs,dialog.ze+1);
        V3DLONG in_sz[4];

        in_sz[0] = dialog.xe - dialog.xs + 1;
        in_sz[1] = dialog.ye - dialog.ys + 1;
        in_sz[2] = dialog.ze - dialog.zs + 1;
        in_sz[3] = in_zz[3];

        Image4DSimple * new4DImage = new Image4DSimple();
        new4DImage->createImage(in_sz[0], in_sz[1], in_sz[2], in_sz[3], V3D_UINT8);
        memcpy(new4DImage->getRawData(), (unsigned char *)cropped_image, new4DImage->getTotalBytes());
        v3dhandle newwin = callback.newImageWindow();
        callback.setImage(newwin, new4DImage);
        callback.setImageName(newwin, "Cropped 3D image");
        callback.updateImageWindow(newwin);
        return;
    }
    else
	{
        v3d_msg(tr("This is plugin to generate 3D stack from image series,"
			"Developed by Zhi Zhou, 2014-06-30"));
	}
}

bool cropped3DImageSeries::dofunc(const QString & func_name, const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 & callback,  QWidget * parent)
{
    if (func_name == tr("crop3d_imageseries"))
	{
        cout<<"Welcome to crop3DImageSeries plugin"<<endl;
        if (output.size() != 1) return false;
        char * inimg_file = ((vector<char*> *)(input.at(0).p))->at(0);
        char * outimg_file = ((vector<char*> *)(output.at(0).p))->at(0);
        long xs, ys, zs, xe, ye, ze;
        int c;
        if (input.size() >= 2)
        {
            vector<char*> paras = (*(vector<char*> *)(input.at(1).p));
            if(paras.size() < 7)
            {
                cout<<"Do not have enought input paremeters"<<endl;
                return false;
            }
            cout<<paras.size()<<endl;
            if(paras.size() >= 1) xs = atoi(paras.at(0)) - 1;
            if(paras.size() >= 2) xe = atoi(paras.at(1)) - 1;
            if(paras.size() >= 3) ys = atoi(paras.at(2)) - 1;
            if(paras.size() >= 4) ye = atoi(paras.at(3)) - 1;
            if(paras.size() >= 5) zs = atoi(paras.at(4)) - 1;
            if(paras.size() >= 6) ze = atoi(paras.at(5)) - 1;
            if(paras.size() >= 7) c = atoi(paras.at(6)) - 1;
         }
        else
        {
            cout<<"Do not have enought input paremeters"<<endl;
            return false;
        }

        cout<<"xs = "<<xs+1<<"; xe = "<<xe+1<<endl;
        cout<<"ys = "<<ys+1<<"; ye = "<<ye+1<<endl;
        cout<<"zs = "<<zs+1<<"; ze = "<<ze+1<<endl;
        cout<<"ch = "<<c+1<<endl;
        cout<<"inimg_folder = "<<inimg_file<<endl;
        cout<<"outimg_file = "<<outimg_file<<endl;

        QString m_InputfolderName(inimg_file);
        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();
        unsigned char * data1d_1st = 0;
        V3DLONG in_sz[4];
        int datatype;

        if (!simple_loadimage_wrapper(callback,const_cast<char *>(vim.tilesList.at(0).fn_image.c_str()), data1d_1st, in_sz, datatype))
        {
            fprintf (stderr, "Error happens in reading the subject file [%0]. Exit. \n",vim.tilesList.at(0).fn_image.c_str());
            return false;
        }
        if(data1d_1st) {delete []data1d_1st; data1d_1st = 0;}

        V3DLONG sz[4];
        sz[0] = in_sz[0];
        sz[1] = in_sz[1];
        sz[2] = NTILES;
        sz[3] = in_sz[3];


        V3DLONG im_cropped_sz[4];
        im_cropped_sz[0] = xe - xs + 1;
        im_cropped_sz[1] = ye - ys + 1;
        im_cropped_sz[2] = ze - zs + 1;
        if(c == -1)
            im_cropped_sz[3] = in_sz[3];
        else
            im_cropped_sz[3] = 1;

        unsigned char *im_cropped = 0;
        V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
        try {im_cropped = new unsigned char [pagesz];}
        catch(...)  {v3d_msg("cannot allocate memory for image_mip."); return false;}

        V3DLONG i = 0;
        for(V3DLONG ii = zs; ii <= ze; ii++)
        {
            unsigned char * data1d = 0;
            V3DLONG in_sz[4];
            int datatype;

            if (!simple_loadimage_wrapper(callback,const_cast<char *>(vim.tilesList.at(ii).fn_image.c_str()), data1d, in_sz, datatype))
            {
                fprintf (stderr, "Error happens in reading the subject file [%0]. Exit. \n",vim.tilesList.at(ii).fn_image.c_str());
                return false;
            }
            V3DLONG offsetc;
            if(c != -1)
            {
                offsetc = c * in_sz[0] *in_sz[1];
                for(V3DLONG iy =  ys; iy <=  ye; iy++)
                {
                    V3DLONG offsetj = iy*in_sz[0];
                    for(V3DLONG ix =  xs; ix <=  xe; ix++)
                    {
                        im_cropped[i] = data1d[offsetc + offsetj + ix];
                        i++;
                    }
                }
            }
            else
            {
                V3DLONG offsetk_cropped = (ii -  zs) *im_cropped_sz[0] * im_cropped_sz[1];
                for(V3DLONG ic = 0; ic < in_sz[3]; ic++)
                {
                    offsetc = ic * in_sz[0] *in_sz[1] * in_sz[2];
                    V3DLONG offsetc_cropped = ic * im_cropped_sz[0] * im_cropped_sz[1] * im_cropped_sz[2];
                    for(V3DLONG iy =  ys; iy <=  ye; iy++)
                    {
                        V3DLONG offsetj = iy*in_sz[0];
                        V3DLONG offsetj_cropped = (iy -  ys) *im_cropped_sz[0];
                        for(V3DLONG ix =  xs; ix <=  xe; ix++)
                        {
                            im_cropped[offsetc_cropped + offsetk_cropped + offsetj_cropped + ix -  xs] = data1d[offsetc + offsetj + ix];
                        }
                    }
                }
            }

            if(data1d) {delete []data1d; data1d = 0;}
        }

        simple_saveimage_wrapper(callback, outimg_file,(unsigned char *)im_cropped,im_cropped_sz,datatype);
        if(im_cropped) {delete []im_cropped; im_cropped = 0;}
        return true;
    }
    else if (func_name == "crop3d_raw")
    {
        cout<<"Welcome to crop3DImageSeries plugin"<<endl;
        if (output.size() != 1) return false;
        char * inimg_file = ((vector<char*> *)(input.at(0).p))->at(0);
        char * outimg_file = ((vector<char*> *)(output.at(0).p))->at(0);
        long xs, ys, zs, xe, ye, ze;
        int c;
        if (input.size() >= 2)
        {
            vector<char*> paras = (*(vector<char*> *)(input.at(1).p));
            if(paras.size() < 7)
            {
                cout<<"Do not have enought input paremeters"<<endl;
                return false;
            }
            cout<<paras.size()<<endl;
            if(paras.size() >= 1) xs = atoi(paras.at(0)) - 1;
            if(paras.size() >= 2) xe = atoi(paras.at(1)) - 1;
            if(paras.size() >= 3) ys = atoi(paras.at(2)) - 1;
            if(paras.size() >= 4) ye = atoi(paras.at(3)) - 1;
            if(paras.size() >= 5) zs = atoi(paras.at(4)) - 1;
            if(paras.size() >= 6) ze = atoi(paras.at(5)) - 1;
            if(paras.size() >= 7) c = atoi(paras.at(6)) - 1;
         }
        else
        {
            cout<<"Do not have enought input paremeters"<<endl;
            return false;
        }

        cout<<"xs = "<<xs+1<<"; xe = "<<xe+1<<endl;
        cout<<"ys = "<<ys+1<<"; ye = "<<ye+1<<endl;
        cout<<"zs = "<<zs+1<<"; ze = "<<ze+1<<endl;
        cout<<"ch = "<<c+1<<endl;
        cout<<"inimg_folder = "<<inimg_file<<endl;
        cout<<"outimg_file = "<<outimg_file<<endl;


        unsigned char * datald = 0;
        V3DLONG *in_zz = 0;
        V3DLONG *in_sz = 0;

        int datatype;

        if (!loadRawRegion(inimg_file, datald, in_zz, in_sz,datatype,0,0,0,1,1,1))
        {
            return false;
        }

        if(datald) {delete []datald; datald = 0;}
        unsigned char * cropped_image = 0;

        if (!loadRawRegion(inimg_file, cropped_image, in_zz, in_sz,datatype,xs,ys,zs,xe+1,ye+1,ze+1))
        {
            return false;
        }

        if(c !=-1 && in_sz[3] > 1)
        {
            void* cropped_image_1channel = 0;
            switch(datatype)
            {
                case 1: extract_a_channel(cropped_image, in_sz, c, cropped_image_1channel); break;
                case 2: extract_a_channel((unsigned short int *)cropped_image, in_sz, c, cropped_image_1channel); break;
                case 4: extract_a_channel((float *)cropped_image, in_sz, c, cropped_image_1channel); break;
                default:
                v3d_msg("Right now this plugin supports only UINT8/UINT16/FLOAT32 data. Do nothing."); return false;
            }

            in_sz[3] = 1;
            simple_saveimage_wrapper(callback, outimg_file,(unsigned char *)cropped_image_1channel,in_sz,datatype);
            if(cropped_image) {delete []cropped_image; cropped_image = 0;}
            if(cropped_image_1channel) {delete []cropped_image_1channel; cropped_image_1channel = 0;}
        }
        else
        {
            simple_saveimage_wrapper(callback, outimg_file,(unsigned char *)cropped_image,in_sz,datatype);
            if(cropped_image) {delete []cropped_image; cropped_image = 0;}
        }
    }
	else if (func_name == tr("cropTerafly"))
	{
        vector<char*> infiles, inparas;
        DataFlow *outdataflow;
		if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
		if(input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
        if(output.size() >= 1) outdataflow = (DataFlow *)output.at(0).p;
        int isout=1;
        isout=atoi(inparas.at(0));
        QString length1 = inparas.at(1);
        QString length2 = inparas.at(2);
        QString length3 = inparas.at(3);
		QString m_InputfolderName = infiles.at(0);
		QString inputListFile = infiles.at(1);
        QString savePath ;//= infiles.at(2);
        if(isout==1){
            savePath = infiles.at(2);
        }
		
        V3DLONG *in_zz;
        if(!callback.getDimTeraFly(m_InputfolderName.toStdString(), in_zz))
        {
            v3d_msg("Cannot load terafly images.",0);
            return false;
        }

		long cubeSideLength1 = length1.toLong();
		long cubeSideLength2 = length2.toLong();
		long cubeSideLength3 = length3.toLong();
		V3DLONG in_sz[4];
		in_sz[0] = cubeSideLength1;
		in_sz[1] = cubeSideLength2;
		in_sz[2] = cubeSideLength3;
		in_sz[3] = in_zz[3];

		QFile inputFile(inputListFile);
		QTextStream in(&inputFile);
		int count = 0;
		if (inputFile.open(QIODevice::ReadOnly))
		{
			while (!in.atEnd())
			{
				QString line = in.readLine();
				QList<QString> coords = line.split(",");
				if (coords[0] == "##n") continue;
//				coords[4] = coords[4].remove(0, 1);
                coords[4] = coords[4].remove(" ");
                coords[5] = coords[5].remove(" ");
                coords[6] = coords[6].remove(" "); //changed by zx 20201210
				qDebug() << coords[4] << " " << coords[5] << " " << coords[6];

//                QString saveName = savePath + "/" + coords[5] + "_" + coords[6] + "_" + coords[4] + ".v3draw";

//                if(QFile::exists(saveName)){
//                    qDebug()<<saveName<<" is exist!";
//                    continue;
//                }

//				QList<QString> zC = coords[4].split(".");
//				QList<QString> xC = coords[5].split(".");
//				QList<QString> yC = coords[6].split(".");

//				long zcenter = zC[0].toLong() - 1;
//				long xcenter = xC[0].toLong() - 1;
//				long ycenter = yC[0].toLong() - 1;
//				cout << zcenter << " " << xcenter << " " << ycenter << endl;

                //changed by zx 20210303

                long zC  = round(coords[4].toFloat());
                long xC  = round(coords[5].toFloat());
                long yC  = round(coords[6].toFloat());

                long zcenter = zC - 1;
                long xcenter = xC - 1;
                long ycenter = yC - 1;
                cout << zcenter << " " << xcenter << " " << ycenter << endl;

                QString saveName ;//= savePath + "/" + QString::number(xC) + "_" + QString::number(yC) + "_" + QString::number(zC) + ".v3draw";
                if(isout==1){
                    saveName = savePath + "/" + QString::number(xC) + "_" + QString::number(yC) + "_" + QString::number(zC) + ".v3draw";
                    if(QFile::exists(saveName)){
                        qDebug()<<saveName<<" is exist!";
                        continue;
                    }
                }
                V3DLONG x0 = xcenter-cubeSideLength2/2;
                V3DLONG x1 = xcenter+cubeSideLength2/2;
                V3DLONG y0 = ycenter-cubeSideLength1/2;
                V3DLONG y1 = ycenter+cubeSideLength1/2;
                V3DLONG z0 = zcenter-cubeSideLength3/2;
                V3DLONG z1 = zcenter+cubeSideLength3/2;
                cout<<"in_zz: "<<in_zz[0]<<" "<<in_zz[1]<<" "<<in_zz[2]<<endl;
                cout<<"x0 x1 y0 y1 z0 z1: "<<x0<<" "<<x1<<" "<<y0<<" "<<y1<<" "<<z0<<" "<<z1<<endl;
                if(x0<0 || x1>=in_zz[0] || y0<0 || y1>=in_zz[1] || z0<0 || z1>=in_zz[2]){
                    qDebug()<<"the cordinate is out of size";
//                    continue;
                }
//                if(x0<0) x0 = 0; if(x0>=in_zz[0]) x0 = in_zz[0]-1;
//                if(x1<0) x1 = 0; if(x1>=in_zz[0]) x1 = in_zz[0]-1;
//                if(y0<0) y0 = 0; if(y0>=in_zz[1]) y0 = in_zz[1]-1;
//                if(y1<0) y1 = 0; if(y1>=in_zz[1]) y1 = in_zz[1]-1;
//                if(z0<0) z0 = 0; if(z0>=in_zz[2]) z0 = in_zz[2]-1;
//                if(z1<0) z1 = 0; if(z1>=in_zz[2]) z1 = in_zz[2]-1; //added by zx

				unsigned char * cropped_image = 0;
                cropped_image = callback.getSubVolumeTeraFly(m_InputfolderName.toStdString(),
                                                               x0, x1,
                                                               y0, y1,
                                                               z0, z1);//changed by zx;
				if(cropped_image==NULL){
					continue;
				}
//				Image4DSimple* new4DImage = new Image4DSimple();
//				new4DImage->createImage(in_sz[0], in_sz[1], in_sz[2], in_sz[3], V3D_UINT8);
                if(isout==1){
                    const char* fileName = saveName.toLatin1();

                    simple_saveimage_wrapper(callback, fileName, cropped_image, in_sz, 1);
                }else{
                    Image4DSimple* nimg=new Image4DSimple();
                    nimg->setData(cropped_image,in_sz[0],in_sz[1],in_sz[2],in_sz[3],V3D_UINT8);
                    outdataflow->push_img(nimg);
                    outdataflow->dataname.push_back(QString::number(xC) + "_" + QString::number(yC) + "_" + QString::number(zC));
                }
                if(isout==1&&cropped_image){
                    delete[] cropped_image;
                }
			}
		}
		inputFile.close();

        return true;
	}
	else if (func_name == tr("teraflyCombine"))
	{
		vector<char*> infiles, inparas, outfiles;
		if (input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p); 
		if (input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
		if (output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);
		
		QString m_InputfolderName = infiles.at(0);
		QString savePath = outfiles.at(0);
		//QString outputFileName = "combinedStack.tif";
		//if (inparas.size() > 0) outputFileName = inparas.at(0);
		//qDebug() << outputFileName;

		V3DLONG* dims;
		if (!callback.getDimTeraFly(m_InputfolderName.toStdString(), dims))
		{
			v3d_msg("Cannot load terafly images.", 0);
			return false;
		}
		
		unsigned char* cropped_image = 0;
		cropped_image = callback.getSubVolumeTeraFly(m_InputfolderName.toStdString(), 0, dims[0], 0, dims[1], 0, dims[2]);
		Image4DSimple* new4DImage = new Image4DSimple();
		new4DImage->createImage(dims[0], dims[1], dims[2], dims[3], V3D_UINT8);
		QString saveName = savePath;
        const char* fileName = saveName.toLatin1();

		simple_saveimage_wrapper(callback, fileName, cropped_image, dims, 1);

		return true;
		
	}
	else if (func_name == tr("slicesFromTera"))
	{
		vector<char*> infiles, inparas, outfiles;
		if (input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
		if (input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
		if (output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

		QString m_InputfolderName = infiles.at(0);
		QString savePath = outfiles.at(0);

		V3DLONG* dims;
		if (!callback.getDimTeraFly(m_InputfolderName.toStdString(), dims))
		{
			v3d_msg("Cannot load terafly images.", 0);
			return false;
		}
		
		int sliceNum = 0;
		while (sliceNum <= dims[2] - 1)
		{
			unsigned char* cropped_image = 0;
			cropped_image = callback.getSubVolumeTeraFly(m_InputfolderName.toStdString(), 0, dims[0], 0, dims[1], sliceNum, sliceNum + 1);
			/*Image4DSimple* new4DImage = new Image4DSimple();
			new4DImage->createImage(dims[0], dims[1], 1, dims[3], V3D_UINT8);*/
			V3DLONG sliceDim[4];
			sliceDim[0] = dims[0];
			sliceDim[1] = dims[1];
			sliceDim[2] = 1;
			sliceDim[3] = dims[3];

			QString prefix;
			if (sliceNum / 10 == 0) prefix = "0000" + QString::number(sliceNum);
			else if (sliceNum / 100 == 0) prefix = "000" + QString::number(sliceNum);
			else if (sliceNum / 1000 == 0) prefix = "00" + QString::number(sliceNum);
			else if (sliceNum / 10000 == 0) prefix = "0" + QString::number(sliceNum);
			else prefix = QString::number(sliceNum);

			QString directory = savePath + "\\" + inparas[0] + "\\";
			QString saveName = savePath + "\\" + inparas[0] + "\\" + prefix + ".tif";
			qDebug() << saveName;
			if (QDir(directory).exists())
			{
                const char* fileName = saveName.toLatin1();
				simple_saveimage_wrapper(callback, fileName, cropped_image, sliceDim, 1);
			}
			else
			{
				QDir().mkdir(directory);
                const char* fileName = saveName.toLatin1();
				simple_saveimage_wrapper(callback, fileName, cropped_image, sliceDim, 1);
			}

			++sliceNum;
		}
		
		return true;
	}
	else if (func_name == tr("cropTerafly_nonApo"))
	{
		vector<char*> infiles, inparas, outfiles;
		if (input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
		if (input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
		if (output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);
		QString m_InputfolderName = infiles.at(0);
		QString savePath = outfiles.at(0);

		V3DLONG *in_zz;
		if (!callback.getDimTeraFly(m_InputfolderName.toStdString(), in_zz))
		{
			v3d_msg("Cannot load terafly images.", 0);
			return false;
        }
		unsigned char* cropped_image = 0;
		cropped_image = callback.getSubVolumeTeraFly(m_InputfolderName.toStdString(), 0, in_zz[0], 0, in_zz[1], 0, in_zz[2]);
        const char* fileName = savePath.toLatin1();
		simple_saveimage_wrapper(callback, fileName, cropped_image, in_zz, 1);
		

		return true;
	}
    else if (func_name == tr("help"))
    {
        cout<<"Usage : v3d -x dllname -f crop3d_imageseries -i <inimg_folder> -o <outimg_file> -p <xs> <xe> <ys> <ye> <zs> <ze> <ch>"<<endl;
        cout<<endl;
        cout<<"xs      minimum in x dimension, start from 1    "<<endl;
        cout<<"xe      maximum in x dimension, start from 1    "<<endl;
        cout<<"ys      minimum in y dimension, start from 1    "<<endl;
        cout<<"ye      maximum in y dimension, start from 1    "<<endl;
        cout<<"zs      minimum in z dimension, start from 1    "<<endl;
        cout<<"ze      maximum in z dimension, start from 1    "<<endl;
        cout<<"ch      the channel value,start from 1 (0 for all channels).  "<<endl;
        cout<<endl;


        cout<<"Usage : v3d -x dllname -f crop3d_raw -i <inimg_folder> -o <outimg_file> -p <xs> <xe> <ys> <ye> <zs> <ze> <ch>"<<endl;
        cout<<endl;
        cout<<"xs      minimum in x dimension, start from 1    "<<endl;
        cout<<"xe      maximum in x dimension, start from 1    "<<endl;
        cout<<"ys      minimum in y dimension, start from 1    "<<endl;
        cout<<"ye      maximum in y dimension, start from 1    "<<endl;
        cout<<"zs      minimum in z dimension, start from 1    "<<endl;
        cout<<"ze      maximum in z dimension, start from 1    "<<endl;
        cout<<"ch      the channel value,start from 1 (0 for all channels).  "<<endl;
        cout<<endl;
		
		cout << "--------------------------------------------------------------" << endl;
		cout << "cropTerafly: This function allows cropping image volumes out of terafly images based on user specified locations. " 
			 << "The input requires a .apo file in which all seed points are supposed to be the centers of cropped image cubes."
			 << "\n\nUsage : v3d -x dllname -f cropTerafly -i <teraFly image folder> <the list file that has soma locations> <save folder of cropped images> -p <cube x side lenth> <cube y side length> <cube z side length>" << endl;
        cout << endl;
        cout << "All folders and files specification require full path input.\n\n";

		cout << "--------------------------------------------------------------" << endl;
		cout << "teraflyCombine: This function assembles tera-cubes under the same resolution level to a single image stack (.tif)" << endl;
		cout << "Usage: v3d -x dllname -f teraflyCombine -i <selected teraFly resolution folder> -o <output path>" << endl << endl;
	}
    else
        return false;

	return true;
}

template <class T> bool extract_a_channel(T* data1d, V3DLONG *sz, V3DLONG c, void* &outimg)
{
    if (!data1d || !sz || c<0 || c>=sz[3])
    {
        printf("problem: c=[%ld] sz=[%p] szc=[%ld], data1d=[%p]\n",c, sz, sz[3], data1d);
        return false;
    }

    outimg = (void *) (data1d + c*sz[0]*sz[1]*sz[2]);

//    printf("ok c=[%ld]\n",c);
    return true;
}

