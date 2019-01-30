#include "tgaimage.h"
#include "model.h"
#include "Point.h"
#include "Vector.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <chrono>

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green   = TGAColor(0, 255,   0,   255);
const int WIDTH = 1600;
const int HEIGHT = 1600;
int width_TEXTURE, height_TEXTURE;
const float light[3] = {0.,0.,1.};

std::vector<std::vector<int>> line(int x0, int y0, int x1, int y1, TGAImage &image, TGAColor color) {
    std::vector<int> tabX, tabY;
    std::vector<std::vector<int>> res;
    bool steep = false;
    if (std::abs(x0-x1)<std::abs(y0-y1)) {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0>x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    int dx = x1-x0;
    int dy = y1-y0;
    int derror2 = std::abs(dy)*2;
    int error2 = 0;
    int y = y0;
    for (int x=x0; x<=x1; x++) {
        if (steep) {
            image.set(y, x, color);
            tabX.push_back(y);
            tabY.push_back(x);
        } else {
            image.set(x, y, color);
            tabX.push_back(x);
            tabY.push_back(y);
        }
        error2 += derror2;
        if (error2 > dx) {
            y += (y1>y0?1:-1);
            error2 -= dx*2;
        }

    }
    res.push_back(tabX);
    res.push_back(tabY);
    return res;
}

std::vector<std::string> readFile(std::string file){
    std::ifstream fichier(file);
    std::vector<std::string> line;
    if(fichier){
        std::string ligne;
        while(std::getline(fichier, ligne)){
            //split each string into 4
            std::istringstream iss(ligne);
            std::vector<std::string> results((std::istream_iterator<std::string>(iss)),std::istream_iterator<std::string>());
            //add the 4 new strings into array
            for(int i = 0 ; i<results.size(); i++){
                line.push_back(results[i]);
            }
        }
    }
    return line;
}

Vector barycentrePoint(Point p0, Point p1, Point p2, Point p) {
    Vector res[2];
    res[0].x = p2.getX() - p0.getX();
    res[0].y = p1.getX() - p0.getX();
    res[0].z = p0.getX() - p.getX();

    res[1].x = p2.getY() - p0.getY();
    res[1].y = p1.getY() - p0.getY();
    res[1].z = p0.getY() - p.getY();

    Vector prodVec; //cross(s[0], s[1]);
    prodVec.x = res[0].getY()*res[1].getZ() - res[0].getZ()*res[1].getY();
    prodVec.y = res[0].getZ()*res[1].getX() - res[0].getX()*res[1].getZ();
    prodVec.z = res[0].getX()*res[1].getY() - res[0].getY()*res[1].getX();

    Vector bary(-1,-1,-1);
    if (std::abs(prodVec.getZ()) > 1e-2) { // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        bary = Vector(1.f - (prodVec.x + prodVec.y) / prodVec.z, prodVec.y / prodVec.z, prodVec.x / prodVec.z);
    }
    return bary;
}

void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, TGAImage &image, TGAColor color){
        if(y1 > y2) {
            std::swap(y1,y2);
            std::swap(x1,x2);
        }
        if(y1 > y3){
            std::swap(y1,y3);
            std::swap(x1,x3);
        }
        if(y2 > y3){
            std::swap(y2,y3);
            std::swap(x2,x3);
        }
        int height = y3 - y1;
        if(height != 0) {
            for (int i = y1; i <= y2; i++) {
                int segment_height = y2 - y1 + 1;
                float a = (float) (i - y1) / height;
                float b = (float) (i - y1) / segment_height;
                int ax = x1 + (x3 - x1) * a;
                int bx = x1 + (x2 - x1) * b;
                if (ax > bx)
                    std::swap(ax, bx);
                //image.set(ax,i,red);
                //image.set(bx,i,green);
                for (int ii = ax; ii <= bx; ii++) {
                    image.set(ii, i, color);
                }
            }
            for (int j = y2; j <= y3; j++) {
                int segment_height = y3 - y2 + 1;
                float a = (float) (j - y1) / height;
                float b = (float) (j - y2) / segment_height;
                int ax = x1 + (x3 - x1) * a;
                int bx = x2 + (x3 - x2) * b;
                if (ax > bx)
                    std::swap(ax, bx);
                for (int jj = ax; jj <= bx; jj++) {
                    image.set(jj, j, color);
                }
            }
        }
}

void fillTrianglePoint(Point p0, Point p1, Point p2, Vector uv1, Vector uv2, Vector uv3, float zbuffer[], TGAImage &image, model m, float intensity){
    /*
    Vector vec1(p0.getX(), p0.getY(), p0.getZ());
    Vector vec2(p1.getX(), p1.getY(), p1.getZ());
    Vector vec3(p2.getX(), p2.getY(), p2.getZ());
    */

    std::vector<Point> vectors;
    vectors.push_back(p0);vectors.push_back(p1);vectors.push_back(p2);

    float boxmin[2];
    boxmin[0] = std::numeric_limits<float>::max();
    boxmin[1] = std::numeric_limits<float>::max();
    float boxmax[2];
    boxmax[0] = -std::numeric_limits<float>::max();
    boxmax[1] = -std::numeric_limits<float>::max();
    Vector pointTexture;

    float tailleImg[2]; tailleImg[0] = WIDTH-1; tailleImg[1] = HEIGHT-1;

    for(int i=0; i<3; i++){
            boxmin[0] = std::max(0.f, std::min( boxmin[0],(float)vectors[i].getX() ) );
            boxmax[0] = std::min(tailleImg[0], std::max( boxmax[0], (float)vectors[i].getX() ) );

            boxmin[1] = std::max(0.f, std::min( boxmin[1],(float)vectors[i].getY() ) );
            boxmax[1] = std::min(tailleImg[1], std::max( boxmax[1], (float)vectors[i].getY() ) );
    }
    Point p;
    for(p.x = boxmin[0]; p.x <= boxmax[0]; p.x++){
        for(p.y = boxmin[1]; p.y <= boxmax[1]; p.y++){
            Vector bary = barycentrePoint(vectors[0], vectors[1], vectors[2], p);
            if(bary.getX() < 0 || bary.getY() < 0 || bary.getZ() < 0){
                continue;
            }

            p.z = 0;
            p.z = p.z + vectors[0].getZ() * bary.getX();
            p.z = p.z + vectors[1].getZ() * bary.getY();
            p.z = p.z + vectors[2].getZ() * bary.getZ();

            if(zbuffer[(p.getX()+p.getY()*WIDTH)] < p.getZ()){
                zbuffer[(p.getX()+p.getY()*WIDTH)] = p.getZ();
                pointTexture.x = uv1.x * bary.x + uv2.x * bary.y + uv3.x * bary.z;
                pointTexture.y = uv1.y * bary.x + uv2.y * bary.y + uv3.y * bary.z;
                TGAColor color = m.imgText.get(pointTexture.x*width_TEXTURE, pointTexture.y*height_TEXTURE);
                for(int i = 0;i<3;i++){
                    color.bgra[i] = color.bgra[i]*intensity;
                }
                image.set(p.x, p.y, color);
            }
        }
    }
}

void drawVertice(model m, TGAImage &image, TGAColor color){
    std::cout << "nverts : " << m.nverts() << std::endl;
    for(int i = 1; i <= m.nverts(); i++){
        //std::cout << "ok"+i << std::endl;
        std::vector<float> vertice = m.vert(i);
        image.set(vertice[0]*(image.get_width()/2)+(image.get_width()/2), vertice[1]*(image.get_height()/2)+(image.get_height()/2), color);
    }
}

void drawTriangle(model m, TGAImage &image, TGAColor color, float zbuffer[]){
    for(int j = 1; j <= m.nfaces(); j++){
        //std::cout << j << "/" << m.nfaces() << std::endl;
        //WORLD COORDINATES
        std::vector<int> face = m.face(j);

        //SCREEN COORDINATES
        Point p0((int)(m.vert(face[0])[0]*(image.get_width()/2)+(image.get_width()/2)), (int)(m.vert(face[0])[1]*(image.get_height()/2)+(image.get_height()/2)),(int)(m.vert(face[0])[2]*(image.get_height()/2)+(image.get_height()/2)));
        Point p1((int)(m.vert(face[1])[0]*(image.get_width()/2)+(image.get_width()/2)), (int)(m.vert(face[1])[1]*(image.get_height()/2)+(image.get_height()/2)),(int)(m.vert(face[1])[2]*(image.get_height()/2)+(image.get_height()/2)));
        Point p2((int)(m.vert(face[2])[0]*(image.get_width()/2)+(image.get_width()/2)), (int)(m.vert(face[2])[1]*(image.get_height()/2)+(image.get_height()/2)),(int)(m.vert(face[2])[2]*(image.get_height()/2)+(image.get_height()/2)));

        //Take 2 vector from triangle :
        Vector vector1(p1.getX() - p0.getX(), p1.getY() - p0.getY(), p1.getZ() - p0.getZ());
        Vector vector2(p2.getX() - p1.getX(), p2.getY() - p1.getY(), p2.getZ() - p1.getZ());

        //Do cross-product :
        float normal_surface[3];

        normal_surface[0] = ( vector1.getY()*vector2.getZ() ) - ( vector1.getZ()*vector2.getY() );
        normal_surface[1] = ( vector1.getZ()*vector2.getX() ) - ( vector1.getX()*vector2.getZ() );
        normal_surface[2] = ( vector1.getX()*vector2.getY() ) - ( vector1.getY()*vector2.getX() );

        //Calculate norme :
        float norme = sqrtf(normal_surface[0]*normal_surface[0] + normal_surface[1]*normal_surface[1] + normal_surface[2]*normal_surface[2]);
        normal_surface[0] = normal_surface[0] / norme;
        normal_surface[1] = normal_surface[1] / norme;
        normal_surface[2] = normal_surface[2] / norme;


        //Calculate intensity (scalaire) :
        float intensity = normal_surface[0]*light[0] + normal_surface[1]*light[1] + normal_surface[2]*light[2];
        //std::cout << "n z : " << normal_surface[2] << " intensite : " << intensity << std::endl;

        //Draw :
        if(intensity > 0) {
            std::vector<Vector> uv = m.uv(j);
            fillTrianglePoint(p0, p1, p2, uv[0], uv[1], uv[2], zbuffer, image, m, intensity);
        }
    }
}

/*
void draw(model m, TGAImage &image, TGAColor color){
    drawVertice(m,image,color);
    drawTriangle(m,image,color);
}
*/


int main(int argc, char** argv) {
    //std::vector<std::string> tab = readFile("../african_head.obj");
    /*
    for(int i =1; i<tab.size(); i++){
        std::cout << tab[i] << std::endl;
    }
    */
    //model m("../african_head.obj");
    model m("../diablo_pose.obj");
    width_TEXTURE = m.imgText.get_width();
    height_TEXTURE = m.imgText.get_height();

    auto start = std::chrono::system_clock::now();
    std::time_t startt = std::chrono::system_clock::to_time_t(start);
    std::cout << std::ctime(&startt) << std::endl;

    //Zbuffer
    float *zbuffer = new float[WIDTH*HEIGHT];
    for(int i = 0; i < WIDTH*HEIGHT; i++){
        zbuffer[i] = -std::numeric_limits<float>::max();
    }

    TGAImage image(HEIGHT,WIDTH, TGAImage::RGB);
    drawTriangle(m,image, white, zbuffer);

    auto step = std::chrono::system_clock::now();
    std::time_t step2 = std::chrono::system_clock::to_time_t(step);
    std::cout << "FIN : " << std::ctime(&step2) << std::endl;

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("../output.tga");
    return 0;
}