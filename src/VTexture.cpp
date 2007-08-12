/*=============================================================================
XMOTO

This file is part of XMOTO.

XMOTO is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

XMOTO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XMOTO; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

/* 
 *  Handling of textures.
 */
#include "Game.h"
#include "VTexture.h"
#include "Image.h"
#include "VFileIO.h"
#include "helpers/Log.h"

  /*===========================================================================
  Create texture from memory
  ===========================================================================*/
  Texture *TextureManager::createTexture(std::string Name,unsigned char *pcData,int nWidth,int nHeight,bool bAlpha,bool bClamp, FilterMode eFilterMode) {
    /* Name free? */
    if(getTexture(Name) != NULL) {
      Logger::Log("** Warning ** : TextureManager::createTexture() : Name '%s' already in use",Name.c_str());
      throw TextureError("texture naming conflict");
    }

    /* Allocate */
    Texture *pTexture = new Texture;
    pTexture->Name = Name;
    pTexture->nWidth = nWidth;
    pTexture->nHeight = nHeight;
    pTexture->Tag = "";
    pTexture->isAlpha = bAlpha;
    pTexture->pcData = pcData;
#ifdef ENABLE_OPENGL
    pTexture->nID = 0;
#endif
    
#ifdef ENABLE_OPENGL
    /* OpenGL magic */
    GLuint N;
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1,&N);    
    glBindTexture(GL_TEXTURE_2D,N);

    switch(eFilterMode) {
      /* require openGL 1.4 */
      case FM_MIPMAP:
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      break;
      
      case FM_LINEAR:
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      break;

      case FM_NEAREST:
      default:
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      break;
    }
        
    if(bClamp) {
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    }
    else {
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    }

    if(bAlpha) {
      /* Got alpha channel */
      if(eFilterMode == FM_MIPMAP){
	gluBuild2DMipmaps(GL_TEXTURE_2D,4,nWidth,nHeight,GL_RGBA,GL_UNSIGNED_BYTE,pcData);
      }else{
	glTexImage2D(GL_TEXTURE_2D,0,4,nWidth,nHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,pcData);
      }
      pTexture->nSize = nWidth * nHeight * 4;
    }
    else {
      /* Plain RGB */
      if(eFilterMode == FM_MIPMAP){
	gluBuild2DMipmaps(GL_TEXTURE_2D,3,nWidth,nHeight,GL_RGB,GL_UNSIGNED_BYTE,pcData);
      }else{
	glTexImage2D(GL_TEXTURE_2D,0,3,nWidth,nHeight,0,GL_RGB,GL_UNSIGNED_BYTE,pcData);
      }
      pTexture->nSize = nWidth * nHeight * 3;
    }

    glDisable(GL_TEXTURE_2D);
    
    pTexture->nID = N;
#endif
    
    m_nTexSpaceUsage += pTexture->nSize;
#ifdef ENABLE_SDLGFX
        #if SDL_BYTEORDER == SDL_BIG_ENDIAN
            Uint32 rmask = 0xff000000;
            Uint32 gmask = 0x00ff0000;
            Uint32 bmask = 0x0000ff00;
            Uint32 amask = 0x000000ff;
        #else
            Uint32 rmask = 0x000000ff;
            Uint32 gmask = 0x0000ff00;
            Uint32 bmask = 0x00ff0000;
            Uint32 amask = 0xff000000;
        #endif

      if(bAlpha){
        pTexture->surface  = SDL_CreateRGBSurfaceFrom(pcData,nWidth,nHeight,32 /*bitsPerPixel */, nWidth * 4 /*pitch*/,rmask,gmask,bmask,amask);
      } else {
        pTexture->surface  = SDL_CreateRGBSurfaceFrom(pcData,nWidth,nHeight,24 /*bitsPerPixel */, nWidth * 3 /*pitch*/,rmask,gmask,bmask,0);

      }
#else
  pTexture->surface = NULL;
#endif
  
    /* Do it captain */
    m_Textures.push_back( pTexture );
    
    return pTexture;
  }
  
  /*===========================================================================
  Destroy texture - i.e. free it from video memory and our list
  ===========================================================================*/  
  void TextureManager::destroyTexture(Texture *pTexture) {
    if(pTexture != NULL) {
      for(unsigned int i=0;i<m_Textures.size();i++) {
        if(m_Textures[i] == pTexture) {
	  SDL_FreeSurface(pTexture->surface);
#ifdef ENABLE_OPENGL
          glDeleteTextures(1,(GLuint *)&pTexture->nID);
#endif
      
	  //#ifdef ENABLE_SDLGFX
	  //keesj:todo when using SDL surface we cannot delete the image data
	  //this is a problem.
	  //delete [] pc; => it's why i keep pTexture->pcData
	  delete [] pTexture->pcData;
	  //#endif
          m_nTexSpaceUsage -= pTexture->nSize;
          delete pTexture;
          m_Textures.erase(m_Textures.begin() + i);
          return;
        }
      }      
    }
    
    throw TextureError("can't destroy unmanaged texture object");
  }
  
  /*===========================================================================
  Shortcut to loading textures from image files
  ===========================================================================*/  
  Texture *TextureManager::loadTexture(std::string Path,bool bSmall,bool bClamp, FilterMode eFilterMode) {
    /* Check file validity */
    image_info_t ii;
    Img TextureImage;
    Texture *pTexture = NULL;
    
    /* Name it */
    std::string TexName = FS::getFileBaseName(Path);

    /* if the texture is already loaded, return it */
    pTexture = getTexture(TexName);
    if(pTexture != NULL) {
      return pTexture;
    }

    if(TextureImage.checkFile( Path,&ii )) {
      /* Valid texture size? */
      if(ii.nWidth != ii.nHeight) {
        Logger::Log("** Warning ** : TextureManager::loadTexture() : texture '%s' is not square",Path.c_str());
        throw TextureError("texture not square");
      }
      if(!(ii.nWidth == 1 ||
         ii.nWidth == 2 ||
         ii.nWidth == 4 ||
         ii.nWidth == 8 ||
         ii.nWidth == 16 ||
         ii.nWidth == 32 ||
         ii.nWidth == 64 ||
         ii.nWidth == 128 ||
         ii.nWidth == 256 ||
         ii.nWidth == 512 ||
         ii.nWidth == 1024)) {
        Logger::Log("** Warning ** : TextureManager::loadTexture() : texture '%s' size is not power of two",Path.c_str());
        throw TextureError("texture size not power of two");
      }
         
      /* Load it into system memory */
      TextureImage.loadFile(Path,bSmall);
      
      /* Copy it into video memory */
      unsigned char *pc;
      bool bAlpha = TextureImage.isAlpha();
      if(bAlpha){
        pc = TextureImage.convertToRGBA32();
      } else {
        pc = TextureImage.convertToRGB24();
      }
      
      pTexture = createTexture(TexName,pc,TextureImage.getWidth(),TextureImage.getHeight(),bAlpha,bClamp, eFilterMode);
    }
    else {
      Logger::Log("** Warning ** : TextureManager::loadTexture() : texture '%s' not found or invalid",Path.c_str());
      throw TextureError(std::string("invalid or missing texture file (" + Path + ")").c_str());
    }    
    
    return pTexture;
  }
  
  /*===========================================================================
  Get loaded texture by name
  ===========================================================================*/  
  Texture *TextureManager::getTexture(std::string Name) {
    for(unsigned int i=0;i<m_Textures.size();i++)
      if(m_Textures[i]->Name == Name) return m_Textures[i];
    return NULL;
  }

  /*===========================================================================
  Fetch all textures with the given tag 
  ===========================================================================*/
  std::vector<Texture *> TextureManager::fetchTaggedTextures(std::string Tag) {
    std::vector<Texture *> Ret;
    
    for(unsigned int i=0;i<m_Textures.size();i++) {
      if(m_Textures[i]->Tag == Tag) Ret.push_back(m_Textures[i]);
    }
    return Ret;
  }
  
  /*===========================================================================
  Unload everything in a very hateful manner
  ===========================================================================*/
  void TextureManager::unloadTextures(void) {
    while(!m_Textures.empty())
      destroyTexture(m_Textures[0]);
  }
