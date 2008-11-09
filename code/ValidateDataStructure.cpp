/*
---------------------------------------------------------------------------
Open Asset Import Library (ASSIMP)
---------------------------------------------------------------------------

Copyright (c) 2006-2008, ASSIMP Development Team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file Implementation of the post processing step to validate 
 * the data structure returned by Assimp
 */

#include "AssimpPCH.h"

// internal headers
#include "ValidateDataStructure.h"
#include "BaseImporter.h"
#include "fast_atof.h"


// CRT headers
#include <stdarg.h>

using namespace Assimp;

// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
ValidateDSProcess::ValidateDSProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well
ValidateDSProcess::~ValidateDSProcess()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Returns whether the processing step is present in the given flag field.
bool ValidateDSProcess::IsActive( unsigned int pFlags) const
{
	return (pFlags & aiProcess_ValidateDataStructure) != 0;
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::ReportError(const char* msg,...)
{
	ai_assert(NULL != msg);

	va_list args;
	va_start(args,msg);

	char szBuffer[3000];

	int iLen;
	iLen = vsprintf(szBuffer,msg,args);

	if (0 >= iLen)
	{
		// :-) should not happen ...
		throw new ImportErrorException("Idiot ... learn coding!");
	}
	va_end(args);
#ifdef _DEBUG
	aiAssert( false,szBuffer,__LINE__,__FILE__ );
#endif
	throw new ImportErrorException("Validation failed: " + std::string(szBuffer,iLen));
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::ReportWarning(const char* msg,...)
{
	ai_assert(NULL != msg);

	va_list args;
	va_start(args,msg);

	char szBuffer[3000];

	int iLen;
	iLen = vsprintf(szBuffer,msg,args);

	if (0 >= iLen)
	{
		// :-) should not happen ...
		throw new ImportErrorException("Idiot ... learn coding!");
	}
	va_end(args);
	DefaultLogger::get()->warn("Validation warning: " + std::string(szBuffer,iLen));
}

// ------------------------------------------------------------------------------------------------
inline int HasNameMatch(const aiString& in, aiNode* node)
{
	int result = (node->mName == in ? 1 : 0 );
	for (unsigned int i = 0; i < node->mNumChildren;++i)
	{
		result += HasNameMatch(in,node->mChildren[i]);
	}
	return result;
}

// ------------------------------------------------------------------------------------------------
template <typename T>
inline void ValidateDSProcess::DoValidation(T** parray, unsigned int size, 
	const char* firstName, const char* secondName)
{
	// validate all entries
	if (size)
	{
		if (!parray)
		{
			ReportError("aiScene::%s is NULL (aiScene::%s is %i)",
				firstName, secondName, size);
		}
		for (unsigned int i = 0; i < size;++i)
		{
			if (!parray[i])
			{
				ReportError("aiScene::%s[%i] is NULL (aiScene::%s is %i)",
					firstName,i,secondName,size);
			}
			Validate(parray[i]);
		}
	}
}

// ------------------------------------------------------------------------------------------------
template <typename T>
inline void ValidateDSProcess::DoValidationEx(T** parray, unsigned int size, 
	const char* firstName, const char* secondName)
{
	// validate all entries
	if (size)
	{
		if (!parray)
		{
			ReportError("aiScene::%s is NULL (aiScene::%s is %i)",
				firstName, secondName, size);
		}
		for (unsigned int i = 0; i < size;++i)
		{
			if (!parray[i])
			{
				ReportError("aiScene::%s[%i] is NULL (aiScene::%s is %i)",
					firstName,i,secondName,size);
			}
			Validate(parray[i]);

			// check whether there are duplicate names
			for (unsigned int a = i+1; a < size;++a)
			{
				if (parray[i]->mName == parray[a]->mName)
				{
					this->ReportError("aiScene::%s[%i] has the same name as "
						"aiScene::%s[%i]",firstName, i,secondName, a);
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
template <typename T>
inline void ValidateDSProcess::DoValidationWithNameCheck(T** array, 
	unsigned int size, const char* firstName, 
	const char* secondName)
{
	// validate all entries
	DoValidationEx(array,size,firstName,secondName);
	
	for (unsigned int i = 0; i < size;++i)
	{
		int res = HasNameMatch(array[i]->mName,mScene->mRootNode);
		if (!res)
		{
			ReportError("aiScene::%s[%i] has no corresponding node in the scene graph (%s)",
				firstName,i,array[i]->mName.data);
		}
		else if (1 != res)
		{
			ReportError("aiScene::%s[%i]: there are more than one nodes with %s as name",
				firstName,i,array[i]->mName.data);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Executes the post processing step on the given imported data.
void ValidateDSProcess::Execute( aiScene* pScene)
{
	this->mScene = pScene;
	DefaultLogger::get()->debug("ValidateDataStructureProcess begin");
	
	// validate the node graph of the scene
	Validate(pScene->mRootNode);
	
	// at least one of the mXXX arrays must be non-empty or we'll flag 
	// the sebe as invalid
	bool has = false;

	// validate all meshes
	if (pScene->mNumMeshes) 
	{
		has = true;
		DoValidation(pScene->mMeshes,pScene->mNumMeshes,"mMeshes","mNumMeshes");
	}
	else if (!(mScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
	{
		ReportError("aiScene::mNumMeshes is 0. At least one mesh must be there");
	}
	else if (pScene->mMeshes)
	{
		ReportError("aiScene::mMeshes is non-null although there are no meshes");
	}
	
	// validate all animations
	if (pScene->mNumAnimations) 
	{
		has = true;
		DoValidation(pScene->mAnimations,pScene->mNumAnimations,
			"mAnimations","mNumAnimations");
	}
	else if (pScene->mAnimations)
	{
		ReportError("aiScene::mAnimations is non-null although there are no animations");
	}

	// validate all cameras
	if (pScene->mNumCameras) 
	{
		has = true;
		DoValidationWithNameCheck(pScene->mCameras,pScene->mNumCameras,
			"mCameras","mNumCameras");
	}
	else if (pScene->mCameras)
	{
		ReportError("aiScene::mCameras is non-null although there are no cameras");
	}

	// validate all lights
	if (pScene->mNumLights) 
	{
		has = true;
		DoValidationWithNameCheck(pScene->mLights,pScene->mNumLights,
			"mLights","mNumLights");
	}
	else if (pScene->mLights)
	{
		ReportError("aiScene::mLights is non-null although there are no lights");
	}
	
	// validate all materials
	if (pScene->mNumMaterials) 
	{
		has = true;
		DoValidation(pScene->mMaterials,pScene->mNumMaterials,"mMaterials","mNumMaterials");
	}
	else if (!(mScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
	{
		ReportError("aiScene::mNumMaterials is 0. At least one material must be there");
	}
	else if (pScene->mMaterials)
	{
		ReportError("aiScene::mMaterials is non-null although there are no materials");
	}

	if (!has)ReportError("The aiScene data structure is empty");
	DefaultLogger::get()->debug("ValidateDataStructureProcess end");
}

// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiLight* pLight)
{
	if (pLight->mType == aiLightSource_UNDEFINED)
		ReportError("aiLight::mType is aiLightSource_UNDEFINED");

	if (!pLight->mAttenuationConstant &&
		!pLight->mAttenuationLinear   && 
		!pLight->mAttenuationQuadratic)
	{
		ReportWarning("aiLight::mAttenuationXXX - all are zero");
	}

	if (pLight->mAngleInnerCone > pLight->mAngleOuterCone)
		ReportError("aiLight::mAngleInnerCone is larger than aiLight::mAngleOuterCone");

	if (pLight->mColorDiffuse.IsBlack() && pLight->mColorAmbient.IsBlack() 
		&& pLight->mColorSpecular.IsBlack())
	{
		ReportWarning("aiLight::mColorXXX - all are black and won't have any influence");
	}
}
	
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiCamera* pCamera)
{
	if (pCamera->mClipPlaneFar <= pCamera->mClipPlaneNear)
		ReportError("aiCamera::mClipPlaneFar must be >= aiCamera::mClipPlaneNear");

	if (!pCamera->mHorizontalFOV || pCamera->mHorizontalFOV >= (float)AI_MATH_PI)
		ReportError("%f is not a valid value for aiCamera::mHorizontalFOV",pCamera->mHorizontalFOV);
}

// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiMesh* pMesh)
{
	// validate the material index of the mesh
	if (pMesh->mMaterialIndex >= this->mScene->mNumMaterials)
	{
		this->ReportError("aiMesh::mMaterialIndex is invalid (value: %i maximum: %i)",
			pMesh->mMaterialIndex,this->mScene->mNumMaterials-1);
	}

	for (unsigned int i = 0; i < pMesh->mNumFaces; ++i)
	{
		aiFace& face = pMesh->mFaces[i];

		if (pMesh->mPrimitiveTypes)
		{
			switch (face.mNumIndices)
			{
			case 0:
				this->ReportError("aiMesh::mFaces[%i].mNumIndices is 0",i);
			case 1:
				if (0 == (pMesh->mPrimitiveTypes & aiPrimitiveType_POINT))
				{
					this->ReportError("aiMesh::mFaces[%i] is a POINT but aiMesh::mPrimtiveTypes "
						"does not report the POINT flag",i);
				}
				break;
			case 2:
				if (0 == (pMesh->mPrimitiveTypes & aiPrimitiveType_LINE))
				{
					this->ReportError("aiMesh::mFaces[%i] is a LINE but aiMesh::mPrimtiveTypes "
						"does not report the LINE flag",i);
				}
				break;
			case 3:
				if (0 == (pMesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
				{
					this->ReportError("aiMesh::mFaces[%i] is a TRIANGLE but aiMesh::mPrimtiveTypes "
						"does not report the TRIANGLE flag",i);
				}
				break;
			default:
				if (0 == (pMesh->mPrimitiveTypes & aiPrimitiveType_POLYGON))
				{
					this->ReportError("aiMesh::mFaces[%i] is a POLYGON but aiMesh::mPrimtiveTypes "
						"does not report the POLYGON flag",i);
				}
				break;
			};
		}

		if (!face.mIndices)this->ReportError("aiMesh::mFaces[%i].mIndices is NULL",i);
	}

	// positions must always be there ...
	if (!pMesh->mNumVertices || !pMesh->mVertices && !mScene->mFlags)
	{
		this->ReportError("The mesh contains no vertices");
	}

	// if tangents are there there must also be bitangent vectors ...
	if ((pMesh->mTangents != NULL) != (pMesh->mBitangents != NULL))
	{
		this->ReportError("If there are tangents there must also be bitangent vectors");
	}

	// faces, too
	if (!pMesh->mNumFaces || !pMesh->mFaces && !mScene->mFlags)
	{
		this->ReportError("The mesh contains no faces");
	}

	// now check whether the face indexing layout is correct:
	// unique vertices, pseudo-indexed.
	std::vector<bool> abRefList;
	abRefList.resize(pMesh->mNumVertices,false);
	for (unsigned int i = 0; i < pMesh->mNumFaces;++i)
	{
		aiFace& face = pMesh->mFaces[i];
		for (unsigned int a = 0; a < face.mNumIndices;++a)
		{
			if (face.mIndices[a] >= pMesh->mNumVertices)
			{
				this->ReportError("aiMesh::mFaces[%i]::mIndices[%i] is out of range",i,a);
			}
			// the MSB flag is temporarily used by the extra verbose
			// mode to tell us that the JoinVerticesProcess might have 
			// been executed already.
			if ( !(this->mScene->mFlags & AI_SCENE_FLAGS_NON_VERBOSE_FORMAT ) && abRefList[face.mIndices[a]])
			{
				ReportError("aiMesh::mVertices[%i] is referenced twice - second "
					"time by aiMesh::mFaces[%i]::mIndices[%i]",face.mIndices[a],i,a);
			}
			abRefList[face.mIndices[a]] = true;
		}
	}

	// check whether there are vertices that aren't referenced by a face
	bool b = false;
	for (unsigned int i = 0; i < pMesh->mNumVertices;++i)
	{
		if (!abRefList[i])b = true;
	}
	abRefList.clear();
	if (b)ReportWarning("There are unreferenced vertices");

	// texture channel 2 may not be set if channel 1 is zero ...
	{
		unsigned int i = 0;
		for (;i < AI_MAX_NUMBER_OF_TEXTURECOORDS;++i)
		{
			if (!pMesh->HasTextureCoords(i))break;
		}
		for (;i < AI_MAX_NUMBER_OF_TEXTURECOORDS;++i)
			if (pMesh->HasTextureCoords(i))
			{
				ReportError("Texture coordinate channel %i exists "
					"although the previous channel was NULL.",i);
			}
	}
	// the same for the vertex colors
	{
		unsigned int i = 0;
		for (;i < AI_MAX_NUMBER_OF_COLOR_SETS;++i)
		{
			if (!pMesh->HasVertexColors(i))break;
		}
		for (;i < AI_MAX_NUMBER_OF_COLOR_SETS;++i)
			if (pMesh->HasVertexColors(i))
			{
				ReportError("Vertex color channel %i is exists "
					"although the previous channel was NULL.",i);
			}
	}


	// now validate all bones
	if (pMesh->mNumBones)
	{
		if (!pMesh->mBones)
		{
			ReportError("aiMesh::mBones is NULL (aiMesh::mNumBones is %i)",
				pMesh->mNumBones);
		}
		float* afSum = NULL;
		if (pMesh->mNumVertices)
		{
			afSum = new float[pMesh->mNumVertices];
			for (unsigned int i = 0; i < pMesh->mNumVertices;++i)
				afSum[i] = 0.0f;
		}

		// check whether there are duplicate bone names
		for (unsigned int i = 0; i < pMesh->mNumBones;++i)
		{
			if (!pMesh->mBones[i])
			{
				delete[] afSum;
				this->ReportError("aiMesh::mBones[%i] is NULL (aiMesh::mNumBones is %i)",
					i,pMesh->mNumBones);
			}
			Validate(pMesh,pMesh->mBones[i],afSum);

			for (unsigned int a = i+1; a < pMesh->mNumBones;++a)
			{
				if (pMesh->mBones[i]->mName == pMesh->mBones[a]->mName)
				{
					delete[] afSum;
					this->ReportError("aiMesh::mBones[%i] has the same name as "
						"aiMesh::mBones[%i]",i,a);
				}
			}
		}
		// check whether all bone weights for a vertex sum to 1.0 ...
		for (unsigned int i = 0; i < pMesh->mNumVertices;++i)
		{
			if (afSum[i] && (afSum[i] <= 0.995 || afSum[i] >= 1.005))
			{
				ReportWarning("aiMesh::mVertices[%i]: bone weight sum != 1.0 (sum is %f)",i,afSum[i]);
			}
		}
		delete[] afSum;
	}
	else if (pMesh->mBones)
	{
		ReportError("aiMesh::mBones is non-null although there are no bones");
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiMesh* pMesh,
	const aiBone* pBone,float* afSum)
{
	this->Validate(&pBone->mName);

   	if (!pBone->mNumWeights)
	{
		this->ReportError("aiBone::mNumWeights is zero");
	}

	// check whether all vertices affected by this bone are valid
	for (unsigned int i = 0; i < pBone->mNumWeights;++i)
	{
		if (pBone->mWeights[i].mVertexId >= pMesh->mNumVertices)
		{
			this->ReportError("aiBone::mWeights[%i].mVertexId is out of range",i);
		}
		else if (!pBone->mWeights[i].mWeight || pBone->mWeights[i].mWeight > 1.0f)
		{
			this->ReportWarning("aiBone::mWeights[%i].mWeight has an invalid value",i);
		}
		afSum[pBone->mWeights[i].mVertexId] += pBone->mWeights[i].mWeight;
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiAnimation* pAnimation)
{
	this->Validate(&pAnimation->mName);

	// validate all materials
	if (pAnimation->mNumChannels)
	{
		if (!pAnimation->mChannels)
		{
			this->ReportError("aiAnimation::mChannels is NULL (aiAnimation::mNumChannels is %i)",
				pAnimation->mNumChannels);
		}
		for (unsigned int i = 0; i < pAnimation->mNumChannels;++i)
		{
			if (!pAnimation->mChannels[i])
			{
				this->ReportError("aiAnimation::mChannels[%i] is NULL (aiAnimation::mNumChannels is %i)",
					i, pAnimation->mNumChannels);
			}
			this->Validate(pAnimation, pAnimation->mChannels[i]);
		}
	}
	else this->ReportError("aiAnimation::mNumChannels is 0. At least one node animation channel must be there.");

	// Animation duration is allowed to be zero in cases where the anim contains only a single key frame.
	// if (!pAnimation->mDuration)this->ReportError("aiAnimation::mDuration is zero");
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::SearchForInvalidTextures(const aiMaterial* pMaterial,
	const char* szType)
{
	ai_assert(NULL != szType);

	// search all keys of the material ...
	// textures must be specified with rising indices (e.g. diffuse #2 may not be
	// specified if diffuse #1 is not there ...)

	// "$tex.file.<szType>[<index>]"
	char szBaseBuf[512];
	int iLen;
	iLen = ::sprintf(szBaseBuf,"$tex.file.%s",szType);
	if (0 >= iLen)return;

	int iNumIndices = 0;
	int iIndex = -1;
	for (unsigned int i = 0; i < pMaterial->mNumProperties;++i)
	{
		aiMaterialProperty* prop = pMaterial->mProperties[i];
		if (0 == ASSIMP_strincmp( prop->mKey.data, szBaseBuf, iLen ))
		{
			const char* sz = &prop->mKey.data[iLen];
			if (*sz)
			{
				++sz;
				iIndex = std::max(iIndex, (int)strtol10(sz,0));
				++iNumIndices;
			}

			if (aiPTI_String != prop->mType)
				this->ReportError("Material property %s is expected to be a string",prop->mKey.data);
		}
	}
	if (iIndex +1 != iNumIndices)
	{
		this->ReportError("%s #%i is set, but there are only %i %s textures",
			szType,iIndex,iNumIndices,szType);
	}
	if (!iNumIndices)return;

	// now check whether all UV indices are valid ...
	iLen = ::sprintf(szBaseBuf,"$tex.uvw.%s",szType);
	if (0 >= iLen)return;

	bool bNoSpecified = true;
	for (unsigned int i = 0; i < pMaterial->mNumProperties;++i)
	{
		aiMaterialProperty* prop = pMaterial->mProperties[i];
		if (0 == ASSIMP_strincmp( prop->mKey.data, szBaseBuf, iLen ))
		{
			if (aiPTI_Integer != prop->mType || sizeof(int) > prop->mDataLength)
				this->ReportError("Material property %s is expected to be an integer",prop->mKey.data);

			const char* sz = &prop->mKey.data[iLen];
			if (*sz)
			{
				++sz;
				iIndex = strtol10(sz,NULL);
				bNoSpecified = false;

				// ignore UV indices for texture channel that are not there ...
				if (iIndex >= iNumIndices)
				{
					// get the value
					iIndex = *((unsigned int*)prop->mData);

					// check whether there is a mesh using this material
					// which has not enough UV channels ...
					for (unsigned int a = 0; a < mScene->mNumMeshes;++a)
					{
						aiMesh* mesh = this->mScene->mMeshes[a];
						if(mesh->mMaterialIndex == (unsigned int)iIndex)
						{
							int iChannels = 0;
							while (mesh->HasTextureCoords(iChannels))++iChannels;
							if (iIndex >= iChannels)
							{
								this->ReportError("Invalid UV index: %i (key %s). Mesh %i has only %i UV channels",
									iIndex,prop->mKey.data,a,iChannels);
							}
						}
					}
				}
			}
		}
	}
	if (bNoSpecified)
	{
		// Assume that all textures are using the first UV channel
		for (unsigned int a = 0; a < mScene->mNumMeshes;++a)
		{
			aiMesh* mesh = this->mScene->mMeshes[a];
			if(mesh->mMaterialIndex == (unsigned int)iIndex)
			{
				if (!mesh->mTextureCoords[0])
				{
					// This is a special case ... it could be that the
					// original mesh format intended the use of a special
					// mapping here.
					ReportWarning("UV-mapped texture, but there are no UV coords");
				}
			}
		}
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiMaterial* pMaterial)
{
	// check whether there are material keys that are obviously not legal
	for (unsigned int i = 0; i < pMaterial->mNumProperties;++i)
	{
		const aiMaterialProperty* prop = pMaterial->mProperties[i];
		if (!prop)
		{
			this->ReportError("aiMaterial::mProperties[%i] is NULL (aiMaterial::mNumProperties is %i)",
				i,pMaterial->mNumProperties);
		}
		if (!prop->mDataLength || !prop->mData)
		{
			this->ReportError("aiMaterial::mProperties[%i].mDataLength or "
				"aiMaterial::mProperties[%i].mData is 0",i,i);
		}
		// check all predefined types
		if (aiPTI_String == prop->mType)
		{
			// FIX: strings are now stored in a less expensive way ...
			if (prop->mDataLength < sizeof(size_t) + ((const aiString*)prop->mData)->length + 1)
			{
				this->ReportError("aiMaterial::mProperties[%i].mDataLength is "
					"too small to contain a string (%i, needed: %i)",
					i,prop->mDataLength,sizeof(aiString));
			}
			this->Validate((const aiString*)prop->mData);
		}
		else if (aiPTI_Float == prop->mType)
		{
			if (prop->mDataLength < sizeof(float))
			{
				this->ReportError("aiMaterial::mProperties[%i].mDataLength is "
					"too small to contain a float (%i, needed: %i)",
					i,prop->mDataLength,sizeof(float));
			}
		}
		else if (aiPTI_Integer == prop->mType)
		{
			if (prop->mDataLength < sizeof(int))
			{
				this->ReportError("aiMaterial::mProperties[%i].mDataLength is "
					"too small to contain an integer (%i, needed: %i)",
					i,prop->mDataLength,sizeof(int));
			}
		}
		// TODO: check whether there is a key with an unknown name ...
	}

	// make some more specific tests 
	float fTemp;
	int iShading;
	if (AI_SUCCESS == aiGetMaterialInteger( pMaterial,AI_MATKEY_SHADING_MODEL,&iShading))
	{
		switch ((aiShadingMode)iShading)
		{
		case aiShadingMode_Blinn:
		case aiShadingMode_CookTorrance:
		case aiShadingMode_Phong:

			if (AI_SUCCESS != aiGetMaterialFloat(pMaterial,AI_MATKEY_SHININESS,&fTemp))
			{
				this->ReportWarning("A specular shading model is specified but there is no "
					"AI_MATKEY_SHININESS key");
			}
			if (AI_SUCCESS == aiGetMaterialFloat(pMaterial,AI_MATKEY_SHININESS_STRENGTH,&fTemp) && !fTemp)
			{
				this->ReportWarning("A specular shading model is specified but the value of the "
					"AI_MATKEY_SHININESS_STRENGTH key is 0.0");
			}
			break;
		default: ;
		};
	}

	if (AI_SUCCESS == aiGetMaterialFloat( pMaterial,AI_MATKEY_OPACITY,&fTemp))
	{
		if (!fTemp)
			ReportWarning("Material is fully transparent ... are you sure you REALLY want this?");
	}

	// check whether there are invalid texture keys
	SearchForInvalidTextures(pMaterial,"diffuse");
	SearchForInvalidTextures(pMaterial,"specular");
	SearchForInvalidTextures(pMaterial,"ambient");
	SearchForInvalidTextures(pMaterial,"emissive");
	SearchForInvalidTextures(pMaterial,"opacity");
	SearchForInvalidTextures(pMaterial,"shininess");
	SearchForInvalidTextures(pMaterial,"normals");
	SearchForInvalidTextures(pMaterial,"height");
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiTexture* pTexture)
{
	// the data section may NEVER be NULL
	if (!pTexture->pcData)
	{
		this->ReportError("aiTexture::pcData is NULL");
	}
	if (pTexture->mHeight)
	{
		if (!pTexture->mWidth)this->ReportError("aiTexture::mWidth is zero "
			"(aiTexture::mHeight is %i, uncompressed texture)",pTexture->mHeight);
	}
	else 
	{
		if (!pTexture->mWidth)this->ReportError("aiTexture::mWidth is zero (compressed texture)");
		if ('.' == pTexture->achFormatHint[0])
		{
			char szTemp[5];
			szTemp[0] = pTexture->achFormatHint[0];
			szTemp[1] = pTexture->achFormatHint[1];
			szTemp[2] = pTexture->achFormatHint[2];
			szTemp[3] = pTexture->achFormatHint[3];
			szTemp[4] = '\0';

			this->ReportWarning("aiTexture::achFormatHint should contain a file extension "
				"without a leading dot (format hint: %s).",szTemp);
		}
	}

	const char* sz = pTexture->achFormatHint;
 	if (	sz[0] >= 'A' && sz[0] <= 'Z' ||
		sz[1] >= 'A' && sz[1] <= 'Z' ||
		sz[2] >= 'A' && sz[2] <= 'Z' ||
		sz[3] >= 'A' && sz[3] <= 'Z')
	{
		this->ReportError("aiTexture::achFormatHint contains non-lowercase characters");
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiAnimation* pAnimation,
	 const aiNodeAnim* pNodeAnim)
{
	this->Validate(&pNodeAnim->mNodeName);

#if 0
	// check whether there is a bone with this name ...
	unsigned int i = 0;
	for (; i < this->mScene->mNumMeshes;++i)
	{
		aiMesh* mesh = this->mScene->mMeshes[i];
		for (unsigned int a = 0; a < mesh->mNumBones;++a)
		{
			if (mesh->mBones[a]->mName == pNodeAnim->mBoneName)
				goto __break_out;
		}
	}
__break_out:
	if (i == this->mScene->mNumMeshes)
	{
		this->ReportWarning("aiNodeAnim::mBoneName is \"%s\". However, no bone with this name was found",
			pNodeAnim->mBoneName.data);
	}
	if (!pNodeAnim->mNumPositionKeys && !pNodeAnim->mNumRotationKeys && !pNodeAnim->mNumScalingKeys)
	{
		this->ReportWarning("A bone animation channel has no keys");
	}
#endif
	// otherwise check whether one of the keys exceeds the total duration of the animation
	if (pNodeAnim->mNumPositionKeys)
	{
		if (!pNodeAnim->mPositionKeys)
		{
			this->ReportError("aiNodeAnim::mPositionKeys is NULL (aiNodeAnim::mNumPositionKeys is %i)",
				pNodeAnim->mNumPositionKeys);
		}
		double dLast = -0.1;
		for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys;++i)
		{
			if (pNodeAnim->mPositionKeys[i].mTime > pAnimation->mDuration)
			{
				ReportError("aiNodeAnim::mPositionKeys[%i].mTime (%.5f) is larger "
					"than aiAnimation::mDuration (which is %.5f)",i,
					(float)pNodeAnim->mPositionKeys[i].mTime,
					(float)pAnimation->mDuration);
			}
			if (pNodeAnim->mPositionKeys[i].mTime <= dLast)
			{
				ReportWarning("aiNodeAnim::mPositionKeys[%i].mTime (%.5f) is smaller "
					"than aiAnimation::mPositionKeys[%i] (which is %.5f)",i,
					(float)pNodeAnim->mPositionKeys[i].mTime,
					i-1, (float)dLast);
			}
			dLast = pNodeAnim->mPositionKeys[i].mTime;
		}
	}
	// rotation keys
	if (pNodeAnim->mNumRotationKeys)
	{
		if (!pNodeAnim->mRotationKeys)
		{
			this->ReportError("aiNodeAnim::mRotationKeys is NULL (aiNodeAnim::mNumRotationKeys is %i)",
				pNodeAnim->mNumRotationKeys);
		}
		double dLast = -0.1;
		for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys;++i)
		{
			if (pNodeAnim->mRotationKeys[i].mTime > pAnimation->mDuration)
			{
				ReportError("aiNodeAnim::mRotationKeys[%i].mTime (%.5f) is larger "
					"than aiAnimation::mDuration (which is %.5f)",i,
					(float)pNodeAnim->mRotationKeys[i].mTime,
					(float)pAnimation->mDuration);
			}
			if (pNodeAnim->mRotationKeys[i].mTime <= dLast)
			{
				ReportWarning("aiNodeAnim::mRotationKeys[%i].mTime (%.5f) is smaller "
					"than aiAnimation::mRotationKeys[%i] (which is %.5f)",i,
					(float)pNodeAnim->mRotationKeys[i].mTime,
					i-1, (float)dLast);
			}
			dLast = pNodeAnim->mRotationKeys[i].mTime;
		}
	}
	// scaling keys
	if (pNodeAnim->mNumScalingKeys)
	{
		if (!pNodeAnim->mScalingKeys)
		{
			this->ReportError("aiNodeAnim::mScalingKeys is NULL (aiNodeAnim::mNumScalingKeys is %i)",
				pNodeAnim->mNumScalingKeys);
		}
		double dLast = -0.1;
		for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys;++i)
		{
			if (pNodeAnim->mScalingKeys[i].mTime > pAnimation->mDuration)
			{
				ReportError("aiNodeAnim::mScalingKeys[%i].mTime (%.5f) is larger "
					"than aiAnimation::mDuration (which is %.5f)",i,
					(float)pNodeAnim->mScalingKeys[i].mTime,
					(float)pAnimation->mDuration);
			}
			if (pNodeAnim->mScalingKeys[i].mTime <= dLast)
			{
				ReportWarning("aiNodeAnim::mScalingKeys[%i].mTime (%.5f) is smaller "
					"than aiAnimation::mScalingKeys[%i] (which is %.5f)",i,
					(float)pNodeAnim->mScalingKeys[i].mTime,
					i-1, (float)dLast);
			}
			dLast = pNodeAnim->mScalingKeys[i].mTime;
		}
	}

	if (!pNodeAnim->mNumScalingKeys && !pNodeAnim->mNumRotationKeys &&
		!pNodeAnim->mNumPositionKeys)
	{
		ReportError("A node animation channel must have at least one subtrack");
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiNode* pNode)
{
	if (!pNode)this->ReportError("A node of the scenegraph is NULL");
	if (pNode != this->mScene->mRootNode && !pNode->mParent)
		this->ReportError("A node has no valid parent (aiNode::mParent is NULL)");

	this->Validate(&pNode->mName);

	// validate all meshes
	if (pNode->mNumMeshes)
	{
		if (!pNode->mMeshes)
		{
			this->ReportError("aiNode::mMeshes is NULL (aiNode::mNumMeshes is %i)",
				pNode->mNumMeshes);
		}
		std::vector<bool> abHadMesh;
		abHadMesh.resize(this->mScene->mNumMeshes,false);
		for (unsigned int i = 0; i < pNode->mNumMeshes;++i)
		{
			if (pNode->mMeshes[i] >= this->mScene->mNumMeshes)
			{
				this->ReportError("aiNode::mMeshes[%i] is out of range (maximum is %i)",
					pNode->mMeshes[i],this->mScene->mNumMeshes-1);
			}
			if (abHadMesh[pNode->mMeshes[i]])
			{
				this->ReportError("aiNode::mMeshes[%i] is already referenced by this node (value: %i)",
					i,pNode->mMeshes[i]);
			}
			abHadMesh[pNode->mMeshes[i]] = true;
		}
	}
	if (pNode->mNumChildren)
	{
		if (!pNode->mChildren)
		{
			this->ReportError("aiNode::mChildren is NULL (aiNode::mNumChildren is %i)",
				pNode->mNumChildren);
		}
		for (unsigned int i = 0; i < pNode->mNumChildren;++i)
		{
			this->Validate(pNode->mChildren[i]);
		}
	}
}
// ------------------------------------------------------------------------------------------------
void ValidateDSProcess::Validate( const aiString* pString)
{
	if (pString->length > MAXLEN)
	{
		this->ReportError("aiString::length is too large (%i, maximum is %i)",
			pString->length,MAXLEN);
	}
	const char* sz = pString->data;
	while (true)
	{
		if ('\0' == *sz)
		{
			if (pString->length != (unsigned int)(sz-pString->data))
				this->ReportError("aiString::data is invalid: the terminal zero is at a wrong offset");
			break;
		}
		else if (sz >= &pString->data[MAXLEN])
			this->ReportError("aiString::data is invalid. There is no terminal character");
		++sz;
	}
}
