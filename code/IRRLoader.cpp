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

/** @file Implementation of the Irr importer class */

#include "AssimpPCH.h"

#include "IRRLoader.h"
#include "ParsingUtils.h"
#include "fast_atof.h"
#include "GenericProperty.h"

#include "SceneCombiner.h"
#include "StandardShapes.h"

using namespace Assimp;


// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
IRRImporter::IRRImporter()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well 
IRRImporter::~IRRImporter()
{
	// nothing to do here
}

// ------------------------------------------------------------------------------------------------
// Returns whether the class can handle the format of the given file. 
bool IRRImporter::CanRead( const std::string& pFile, IOSystem* pIOHandler) const
{
	/* NOTE: A simple check for the file extension is not enough
	 * here. Irrmesh and irr are easy, but xml is too generic
	 * and could be collada, too. So we need to open the file and
	 * search for typical tokens.
	 */

	std::string::size_type pos = pFile.find_last_of('.');

	// no file extension - can't read
	if( pos == std::string::npos)
		return false;

	std::string extension = pFile.substr( pos);
	for (std::string::iterator i = extension.begin(); i != extension.end();++i)
		*i = ::tolower(*i);

	if (extension == ".irr")return true;
	else if (extension == ".xml")
	{
		/*  If CanRead() is called to check whether the loader
		 *  supports a specific file extension in general we
		 *  must return true here.
		 */
		if (!pIOHandler)return true;
		const char* tokens[] = {"irr_scene"};
		return SearchFileHeaderForToken(pIOHandler,pFile,tokens,1);
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::GetExtensionList(std::string& append)
{
	/*  NOTE: The file extenxsion .xml is too generic. We'll 
	*  need to open the file in CanRead() and check whether it is 
	*  a real irrlicht file
	*/
	append.append("*.xml;*.irr");
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::SetupProperties(const Importer* pImp)
{
	// read the output frame rate of all node animation channels
	fps = pImp->GetPropertyInteger(AI_CONFIG_IMPORT_IRR_ANIM_FPS,100);
	if (!fps)
	{
		DefaultLogger::get()->error("IRR: Invalid FPS configuration");
		fps = 100;
	}
}

// ------------------------------------------------------------------------------------------------
// Build a mesh tha consists of a single squad (a side of a skybox)
aiMesh* IRRImporter::BuildSingleQuadMesh(const SkyboxVertex& v1,
	const SkyboxVertex& v2,
	const SkyboxVertex& v3,
	const SkyboxVertex& v4)
{
	// allocate and prepare the mesh
	aiMesh* out = new aiMesh();

	out->mPrimitiveTypes = aiPrimitiveType_POLYGON;
	out->mNumFaces = 1;

	// build the face
	out->mFaces    = new aiFace[1];
	aiFace& face   = out->mFaces[0];
	
	face.mNumIndices = 4;
	face.mIndices    = new unsigned int[4];
	for (unsigned int i = 0; i < 4;++i)
		face.mIndices[i] = i;

	out->mNumVertices = 4;

	// copy vertex positions
	aiVector3D* vec = out->mVertices = new aiVector3D[4];
	*vec++ = v1.position;
	*vec++ = v2.position;
	*vec++ = v3.position;
	*vec   = v4.position;

	// copy vertex normals
	vec = out->mNormals = new aiVector3D[4];
	*vec++ = v1.normal;
	*vec++ = v2.normal;
	*vec++ = v3.normal;
	*vec   = v4.normal;

	// copy texture coordinates
	out->mTextureCoords[0] = new aiVector3D[4];
	*vec++ = v1.uv;
	*vec++ = v2.uv;
	*vec++ = v3.uv;
	*vec   = v4.uv;

	return out;
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::BuildSkybox(std::vector<aiMesh*>& meshes, std::vector<aiMaterial*> materials)
{
	// Update the material of the skybox - replace the name
	// and disable shading for skyboxes.
	for (unsigned int i = 0; i < 6;++i)
	{
		MaterialHelper* out = ( MaterialHelper* ) (*(materials.end()-(6-i)));

		aiString s;
		s.length = ::sprintf( s.data, "SkyboxSide_%i",i );
		out->AddProperty(&s,AI_MATKEY_NAME);

		int shading = aiShadingMode_NoShading;
		out->AddProperty(&shading,1,AI_MATKEY_SHADING_MODEL);
	}

	// Skyboxes are much more difficult. They are represented
	// by six single planes with different textures, so we'll
	// need to build six meshes.

	const float l = 10.f; // the size used by Irrlicht

	// FRONT SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex(-l,-l,-l,  0, 0, 1,   1.f,1.f),
		SkyboxVertex( l,-l,-l,  0, 0, 1,   0.f,1.f),
		SkyboxVertex( l, l,-l,  0, 0, 1,   0.f,0.f),
		SkyboxVertex(-l, l,-l,  0, 0, 1,   1.f,0.f)) );
	meshes.back()->mMaterialIndex = materials.size()-6u;

	// LEFT SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex( l,-l,-l,  -1, 0, 0,   1.f,1.f),
		SkyboxVertex( l,-l, l,  -1, 0, 0,   0.f,1.f),
		SkyboxVertex( l, l, l,  -1, 0, 0,   0.f,0.f),
		SkyboxVertex( l, l,-l,  -1, 0, 0,   1.f,0.f)) );
	meshes.back()->mMaterialIndex = materials.size()-5u;

	// BACK SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex( l,-l, l,  0, 0, -1,   1.f,1.f),
		SkyboxVertex(-l,-l, l,  0, 0, -1,   0.f,1.f),
		SkyboxVertex(-l, l, l,  0, 0, -1,   0.f,0.f),
		SkyboxVertex( l, l, l,  0, 0, -1,   1.f,0.f)) );
	meshes.back()->mMaterialIndex = materials.size()-4u;

	// RIGHT SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex(-l,-l, l,  1, 0, 0,   1.f,1.f),
		SkyboxVertex(-l,-l,-l,  1, 0, 0,   0.f,1.f),
		SkyboxVertex(-l, l,-l,  1, 0, 0,   0.f,0.f),
		SkyboxVertex(-l, l, l,  1, 0, 0,   1.f,0.f)) );
	meshes.back()->mMaterialIndex = materials.size()-3u;

	// TOP SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex( l, l,-l,  0, -1, 0,   1.f,1.f),
		SkyboxVertex( l, l, l,  0, -1, 0,   0.f,1.f),
		SkyboxVertex(-l, l, l,  0, -1, 0,   0.f,0.f),
		SkyboxVertex(-l, l,-l,  0, -1, 0,   1.f,0.f)) );
	meshes.back()->mMaterialIndex = materials.size()-2u;

	// BOTTOM SIDE
	meshes.push_back( BuildSingleQuadMesh( 
		SkyboxVertex( l,-l, l,  0,  1, 0,   0.f,0.f),
		SkyboxVertex( l,-l,-l,  0,  1, 0,   1.f,0.f),
		SkyboxVertex(-l,-l,-l,  0,  1, 0,   1.f,1.f),
		SkyboxVertex(-l,-l,-l,  0,  1, 0,   0.f,1.f)) );
	meshes.back()->mMaterialIndex = materials.size()-1u;
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::CopyMaterial(std::vector<aiMaterial*>	 materials,
	std::vector< std::pair<aiMaterial*, unsigned int> >& inmaterials,
	unsigned int& defMatIdx,
	aiMesh* mesh)
{
	if (inmaterials.empty())
	{
		// Do we have a default material? If not we need to create one
		if (0xffffffff == defMatIdx)
		{
			defMatIdx = (unsigned int)materials.size();
			MaterialHelper* mat = new MaterialHelper();

			aiString s;
			s.Set(AI_DEFAULT_MATERIAL_NAME);
			mat->AddProperty(&s,AI_MATKEY_NAME);

			aiColor3D c(0.6f,0.6f,0.6f);
			mat->AddProperty(&c,1,AI_MATKEY_COLOR_DIFFUSE);
		}
		mesh->mMaterialIndex = defMatIdx;
		return;
	}
	else if (inmaterials.size() > 1)
	{
		DefaultLogger::get()->info("IRR: Skipping additional materials");
	}

	mesh->mMaterialIndex = (unsigned int)materials.size();
	materials.push_back(inmaterials[0].first);
}


// ------------------------------------------------------------------------------------------------
inline int ClampSpline(int idx, int size)
{
	return ( idx<0 ? size+idx : ( idx>=size ? idx-size : idx ) );
}

// ------------------------------------------------------------------------------------------------
inline void FindSuitableMultiple(int& angle)
{
	if (angle < 3)angle = 3;
	else if (angle < 10) angle = 10;
	else if (angle < 20) angle = 20;
	else if (angle < 30) angle = 30;
	else
	{
	}
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::ComputeAnimations(Node* root, std::vector<aiNodeAnim*>& anims,
	const aiMatrix4x4& transform)
{
	ai_assert(NULL != root);

	if (root->animators.empty())return;

	typedef std::pair< TemporaryAnim, Animator* > AnimPair;
	const unsigned int resolution = 1;

	std::vector<AnimPair> temp;
	temp.reserve(root->animators.size());

	for (std::list<Animator>::iterator it = root->animators.begin();
		it != root->animators.end(); ++it)
	{
		if ((*it).type == Animator::UNKNOWN ||
			(*it).type == Animator::OTHER)
		{
			DefaultLogger::get()->warn("IRR: Skipping unknown or unsupported animator");
			continue;
		}
		temp.push_back(AnimPair(TemporaryAnim(),&(*it)));
	}

	if (temp.empty())return;

	// All animators are applied one after another. We generate a set of
	// transformation matrices for each of it. Then we combine all 
	// transformation matrices, decompose them and build an output animation.

	for (std::vector<AnimPair>::iterator it = temp.begin();
		 it != temp.end(); ++it)
	{
		TemporaryAnim& out = (*it).first;
		Animator* in       = (*it).second;

		switch (in->type)
		{
		case Animator::ROTATION:
			{
				// -----------------------------------------------------
				// find out how long a full rotation will take
				// This is the least common multiple of 360.f and all 
				// three euler angles. Although we'll surely find a 
				// possible multiple (haha) it could be somewhat large
				// for our purposes. So we need to modify the angles
				// here in order to get good results.
				// -----------------------------------------------------
				int angles[3];
				angles[0] = (int)(in->direction.x*100);
				angles[1] = (int)(in->direction.y*100);
				angles[2] = (int)(in->direction.z*100);

				angles[0] %= 360;
				angles[1] %= 360;
				angles[2] %= 360;

				FindSuitableMultiple(angles[0]);
				FindSuitableMultiple(angles[1]);
				FindSuitableMultiple(angles[2]);

				int lcm = 360;

				if (angles[0])
					lcm  = boost::math::lcm(lcm,angles[0]);

				if (angles[1])
					lcm  = boost::math::lcm(lcm,angles[1]);

				if (angles[2])
					lcm  = boost::math::lcm(lcm,angles[2]);

				if (360 == lcm)
					break;

				// This can be a division through zero, but we don't care
				float f1 = (float)lcm / angles[0];
				float f2 = (float)lcm / angles[1];
				float f3 = (float)lcm / angles[2];
				
				// find out how many time units we'll need for the finest
				// track (in seconds) - this defines the number of output
				// keys (fps * seconds)
				float max ;
				if (angles[0])
					max = (float)lcm / angles[0];
				if (angles[1])
					max = std::max(max, (float)lcm / angles[1]);
				if (angles[2])
					max = std::max(max, (float)lcm / angles[2]);

				
				// Allocate transformation matrices
				out.SetupMatrices((unsigned int)(max*fps));

				// begin with a zero angle
				aiVector3D angle;
				for (unsigned int i = 0; i < out.last;++i)
				{
					// build the rotation matrix for the given euler angles
					aiMatrix4x4& m = out.matrices[i];

					// we start with the node transformation
					m = transform; 

					aiMatrix4x4 m2;

					if (angle.x)
						m *= aiMatrix4x4::RotationX(angle.x,m2);

					if (angle.y)
						m *= aiMatrix4x4::RotationX(angle.y,m2);

					if (angle.z)
						m *= aiMatrix4x4::RotationZ(angle.z,m2);

					// increase the angle
					angle += in->direction;
				}

				// This animation is repeated and repeated ...
				out.post = aiAnimBehaviour_REPEAT;
			}
			break;

		case Animator::FLY_CIRCLE:
			{
				
			}
			break;

		case Animator::FLY_STRAIGHT:
			{
				
			}
			break;

		case Animator::FOLLOW_SPLINE:
			{
				out.post = aiAnimBehaviour_REPEAT;
				const int size = (int)in->splineKeys.size();
				if (!size)
				{
					// We have no point in the spline. That's bad. Really bad.
					DefaultLogger::get()->warn("IRR: Spline animators with no points defined");
					break;
				}
				else if (size == 1)
				{
					// We have just one point in the spline
					out.SetupMatrices(1);
					out.matrices[0].a4 = in->splineKeys[0].mValue.x;
					out.matrices[0].b4 = in->splineKeys[0].mValue.y;
					out.matrices[0].c4 = in->splineKeys[0].mValue.z;
					break;
				}

				unsigned int ticksPerFull = 15;
				out.SetupMatrices(ticksPerFull*fps);

				for (unsigned int i = 0; i < out.last;++i)
				{
					aiMatrix4x4& m = out.matrices[i];

					const float dt = (i * in->speed * 0.001f );
					const float u = dt - floor(dt);
					const int idx = (int)floor(dt) % size;

					// get the 4 current points to evaluate the spline
					const aiVector3D& p0 = in->splineKeys[ ClampSpline( idx - 1, size ) ].mValue;
					const aiVector3D& p1 = in->splineKeys[ ClampSpline( idx + 0, size ) ].mValue; 
					const aiVector3D& p2 = in->splineKeys[ ClampSpline( idx + 1, size ) ].mValue; 
					const aiVector3D& p3 = in->splineKeys[ ClampSpline( idx + 2, size ) ].mValue;

					// compute polynomials
					const float u2 = u*u;
					const float u3 = u2*2;

					const float h1 = 2.0f * u3 - 3.0f * u2 + 1.0f;
					const float h2 = -2.0f * u3 + 3.0f * u3;
					const float h3 = u3 - 2.0f * u3;
					const float h4 = u3 - u2;

					// compute the spline tangents
					const aiVector3D t1 = ( p2 - p0 ) * in->tightness;
					aiVector3D t2 = ( p3 - p1 ) * in->tightness;

					// and use them to get the interpolated point
					t2 = (h1 * p1 + p2 * h2 + t1 * h3 + h4 * t2);

					// build a simple translation matrix from it
					m.a4 = t2.x;
					m.b4 = t2.y;
					m.c4 = t2.z;
				}
			}
			break;
		};
	}


	aiNodeAnim* out = new aiNodeAnim();
	out->mNodeName.Set(root->name);

	if (temp.size() == 1)
	{
		// If there's just one animator to be processed our 
		// task is quite easy
		TemporaryAnim& one = temp[0].first;
		
		out->mPostState = one.post;
		out->mNumPositionKeys = one.last;
		out->mNumScalingKeys  = one.last;
		out->mNumRotationKeys = one.last;

		out->mPositionKeys = new aiVectorKey[one.last];
		out->mScalingKeys  = new aiVectorKey[one.last];
		out->mRotationKeys = new aiQuatKey[one.last];

		for (unsigned int i = 0; i < one.last;++i)
		{
			aiVectorKey& scaling  = out->mScalingKeys[i];
			aiVectorKey& position = out->mPositionKeys[i];
			aiQuatKey&   rotation = out->mRotationKeys[i];

			scaling.mTime = position.mTime = rotation.mTime = (double)i;
			one.matrices[i].Decompose(scaling.mValue, rotation.mValue, position.mValue);
		}
	}

	// NOTE: It is possible that some of the tracks we're returning
	// are dummy tracks, but the ScenePreprocessor will fix that, hopefully
}

// ------------------------------------------------------------------------------------------------
void IRRImporter::GenerateGraph(Node* root,aiNode* rootOut ,aiScene* scene,
	BatchLoader& batch,
	std::vector<aiMesh*>&        meshes,
	std::vector<aiNodeAnim*>&    anims,
	std::vector<AttachmentInfo>& attach,
	std::vector<aiMaterial*>	 materials,
	unsigned int&				 defMatIdx)
{
	unsigned int oldMeshSize = (unsigned int)meshes.size();

	// Now determine the type of the node 
	switch (root->type)
	{
	case Node::ANIMMESH:
	case Node::MESH:
		{
			// get the loaded mesh from the scene and add it to
			// the list of all scenes to be attached to the 
			// graph we're currently building
			aiScene* scene = batch.GetImport(root->meshPath);
			if (!scene)
			{
				DefaultLogger::get()->error("IRR: Unable to load external file: " 
					+ root->meshPath);
				break;
			}
			attach.push_back(AttachmentInfo(scene,rootOut));

			// now combine the material we've loaded for this mesh
			// with the real meshes we got from the file. As we
			// don't execute any pp-steps on the file, the numbers
			// should be equal. If they are not, we can impossibly
			// do this  ...
			if (root->materials.size() != (unsigned int)scene->mNumMaterials)
			{
				DefaultLogger::get()->warn("IRR: Failed to match imported materials "
					"with the materials found in the IRR scene file");

				break;
			}
			for (unsigned int i = 0; i < scene->mNumMaterials;++i)
			{
				// delete the old material
				delete scene->mMaterials[i];

				std::pair<aiMaterial*, unsigned int>& src = root->materials[i];
				scene->mMaterials[i] = src.first;
			}

			// NOTE: Each mesh should have exactly one material assigned,
			// but we do it in a separate loop if this behaviour changes
			// in the future.
			for (unsigned int i = 0; i < scene->mNumMeshes;++i)
			{
				// Process material flags 
				aiMesh* mesh = scene->mMeshes[i];


				// If "trans_vertex_alpha" mode is enabled, search all vertex colors 
				// and check whether they have a common alpha value. This is quite
				// often the case so we can simply extract it to a shared oacity
				// value.
				std::pair<aiMaterial*, unsigned int>& src = root->materials[
					mesh->mMaterialIndex];

				MaterialHelper* mat = (MaterialHelper*)src.first;
				if (mesh->HasVertexColors(0) && 
					src.second & AI_IRRMESH_MAT_trans_vertex_alpha)
				{
					bool bdo = true;
					for (unsigned int a = 1; a < mesh->mNumVertices;++a)
					{
						if (mesh->mColors[0][a].a != mesh->mColors[0][a-1].a)
						{
							bdo = false;
							break;
						}
					}
					if (bdo)
					{
						DefaultLogger::get()->info("IRR: Replacing mesh vertex "
							"alpha with common opacity");

						for (unsigned int a = 0; a < mesh->mNumVertices;++a)
							mesh->mColors[0][a].a = 1.f;

						mat->AddProperty(& mesh->mColors[0][0].a, 1, AI_MATKEY_OPACITY);
					}
				}

				// If we have a second texture coordinate set and a second texture
				// (either lightmap, normalmap, 2layered material we need to
				// setup the correct UV index for it). The texture can either
				// be diffuse (lightmap & 2layer) or a normal map (normal & parallax)
				if (mesh->HasTextureCoords(1))
				{
					int idx = 1;
					if (src.second & (AI_IRRMESH_MAT_solid_2layer |
						AI_IRRMESH_MAT_lightmap))
					{
						mat->AddProperty(&idx,1,AI_MATKEY_UVWSRC_DIFFUSE(0));
					}
					else if (src.second & AI_IRRMESH_MAT_normalmap_solid)
					{
						mat->AddProperty(&idx,1,AI_MATKEY_UVWSRC_NORMALS(0));
					}
				}
			}
		}
		break;

	case Node::LIGHT:
	case Node::CAMERA:

		// We're already finished with lights and cameras
		break;


	case Node::SPHERE:
		{
			// generate the sphere model. Our input parameter to
			// the sphere generation algorithm is the number of
			// subdivisions of each triangle - but here we have
			// the number of poylgons on a specific axis. Just
			// use some limits ...
			unsigned int mul = root->spherePolyCountX*root->spherePolyCountY;
			if      (mul < 100)mul = 2;
			else if (mul < 300)mul = 3;
			else               mul = 4;

			meshes.push_back(StandardShapes::MakeMesh(mul,
				&StandardShapes::MakeSphere));

			// Adjust scaling
			root->scaling *= root->sphereRadius;

			// Copy one output material
			CopyMaterial(materials, root->materials, defMatIdx, meshes.back());
		}
		break;

	case Node::CUBE:
		{
			// Generate an unit cube first
			meshes.push_back(StandardShapes::MakeMesh(
				&StandardShapes::MakeHexahedron));

			// Adjust scaling
			root->scaling *= root->sphereRadius;

			// Copy one output material
			CopyMaterial(materials, root->materials, defMatIdx, meshes.back());
		}
		break;


	case Node::SKYBOX:
		{
			// A skybox is defined by six materials
			if (root->materials.size() < 6)
			{
				DefaultLogger::get()->error("IRR: There should be six materials "
					"for a skybox");

				break;
			}

			// copy those materials and generate 6 meshes for our new skybox
			materials.reserve(materials.size() + 6);
			for (unsigned int i = 0; i < 6;++i)
				materials.insert(materials.end(),root->materials[i].first);

			BuildSkybox(meshes,materials);

			// *************************************************************
			// Skyboxes will require a different code path for rendering,
			// so there must be a way for the user to add special support
			// for IRR skyboxes. We add a 'IRR.SkyBox_' prefix to the node.
			// *************************************************************
			root->name = "IRR.SkyBox_" + root->name;
			DefaultLogger::get()->info("IRR: Loading skybox, this will "
				"require special handling to be displayed correctly");
		}
		break;

	case Node::TERRAIN:
		{
			// to support terrains, we'd need to have a texture decoder
			DefaultLogger::get()->error("IRR: Unsupported node - TERRAIN");
		}
		break;
	};

	// Check whether we added a mesh (or more than one ...). In this case 
	// we'll also need to attach it to the node
	if (oldMeshSize != (unsigned int) meshes.size())
	{
		rootOut->mNumMeshes = (unsigned int)meshes.size() - oldMeshSize;
		rootOut->mMeshes    = new unsigned int[rootOut->mNumMeshes];

		for (unsigned int a = 0; a  < rootOut->mNumMeshes;++a)
		{
			rootOut->mMeshes[a] = oldMeshSize+a;
		}
	}

	// Setup the name of this node
	rootOut->mName.Set(root->name);

	// Now compute the final local transformation matrix of the
	// node from the given translation, rotation and scaling values.
	// (the rotation is given in Euler angles, XYZ order)
	aiMatrix4x4 m;
	rootOut->mTransformation = aiMatrix4x4::RotationX(AI_DEG_TO_RAD(root->rotation.x),m)
		* aiMatrix4x4::RotationY(AI_DEG_TO_RAD(root->rotation.y),m)
		* aiMatrix4x4::RotationZ(AI_DEG_TO_RAD(root->rotation.z),m);

	// apply scaling
	aiMatrix4x4& mat = rootOut->mTransformation;
	mat.a1 *= root->scaling.x;
	mat.b1 *= root->scaling.x; 
	mat.c1 *= root->scaling.x;
	mat.a2 *= root->scaling.y; 
	mat.b2 *= root->scaling.y; 
	mat.c2 *= root->scaling.y;
	mat.a3 *= root->scaling.z;
	mat.b3 *= root->scaling.z; 
	mat.c3 *= root->scaling.z;

	// apply translation
	mat.a4 = root->position.x; 
	mat.b4 = root->position.y; 
	mat.c4 = root->position.z;

	// now compute animations for the node
	ComputeAnimations(root,anims,mat);

	// Add all children recursively. First allocate enough storage
	// for them, then call us again
	rootOut->mNumChildren = (unsigned int)root->children.size();
	if (rootOut->mNumChildren)
	{
		rootOut->mChildren = new aiNode*[rootOut->mNumChildren];
		for (unsigned int i = 0; i < rootOut->mNumChildren;++i)
		{
			aiNode* node = rootOut->mChildren[i] =  new aiNode();
			node->mParent = rootOut;
			GenerateGraph(root->children[i],node,scene,batch,meshes,
				anims,attach,materials,defMatIdx);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Imports the given file into the given scene structure. 
void IRRImporter::InternReadFile( const std::string& pFile, 
	aiScene* pScene, IOSystem* pIOHandler)
{
	boost::scoped_ptr<IOStream> file( pIOHandler->Open( pFile));

	// Check whether we can read from the file
	if( file.get() == NULL)
		throw new ImportErrorException( "Failed to open IRR file " + pFile + "");

	// Construct the irrXML parser
	CIrrXML_IOStreamReader st(file.get());
	reader = createIrrXMLReader((IFileReadCallBack*) &st);

	// The root node of the scene
	Node* root = new Node(Node::DUMMY);
	root->parent = NULL;

	// Current node parent
	Node* curParent = root;

	// Scenegraph node we're currently working on
	Node* curNode = NULL;

	// List of output cameras
	std::vector<aiCamera*> cameras;

	// List of output lights
	std::vector<aiLight*> lights;

	// Batch loader used to load external models
	BatchLoader batch(pIOHandler);
	
	cameras.reserve(5);
	lights.reserve(5);

	bool inMaterials = false, inAnimator = false;
	unsigned int guessedAnimCnt = 0, guessedMeshCnt = 0, guessedMatCnt = 0;

	// Parse the XML file
	while (reader->read())
	{
		switch (reader->getNodeType())
		{
		case EXN_ELEMENT:
			
			if (!ASSIMP_stricmp(reader->getNodeName(),"node"))
			{
				// ***********************************************************************
				/*  What we're going to do with the node depends
				 *  on its type:
				 *
				 *  "mesh" - Load a mesh from an external file
				 *  "cube" - Generate a cube 
				 *  "skybox" - Generate a skybox
				 *  "light" - A light source
				 *  "sphere" - Generate a sphere mesh
				 *  "animatedMesh" - Load an animated mesh from an external file
				 *    and join its animation channels with ours.
				 *  "empty" - A dummy node
				 *  "camera" - A camera
				 *
				 *  Each of these nodes can be animated and all can have multiple
				 *  materials assigned (except lights, cameras and dummies, of course).
				 */
				// ***********************************************************************
				const char* sz = reader->getAttributeValueSafe("type");
				Node* nd;
				if (!ASSIMP_stricmp(sz,"mesh"))
				{
					nd = new Node(Node::MESH);
				}
				else if (!ASSIMP_stricmp(sz,"cube"))
				{
					nd = new Node(Node::CUBE);
					++guessedMeshCnt;
					// meshes.push_back(StandardShapes::MakeMesh(&StandardShapes::MakeHexahedron));
				}
				else if (!ASSIMP_stricmp(sz,"skybox"))
				{
					nd = new Node(Node::SKYBOX);
					++guessedMeshCnt;
				}
				else if (!ASSIMP_stricmp(sz,"camera"))
				{
					nd = new Node(Node::CAMERA);

					// Setup a temporary name for the camera
					aiCamera* cam = new aiCamera();
					cam->mName.Set( nd->name );
					cameras.push_back(cam);
				}
				else if (!ASSIMP_stricmp(sz,"light"))
				{
					nd = new Node(Node::LIGHT);

					// Setup a temporary name for the light
					aiLight* cam = new aiLight();
					cam->mName.Set( nd->name );
					lights.push_back(cam);
				}
				else if (!ASSIMP_stricmp(sz,"sphere"))
				{
					nd = new Node(Node::SPHERE);
					++guessedMeshCnt;
				}
				else if (!ASSIMP_stricmp(sz,"animatedMesh"))
				{
					nd = new Node(Node::ANIMMESH);
				}
				else if (!ASSIMP_stricmp(sz,"empty"))
				{
					nd = new Node(Node::DUMMY);
				}
				else
				{
					DefaultLogger::get()->warn("IRR: Found unknown node: " + std::string(sz));

					/*  We skip the contents of nodes we don't know.
					 *  We parse the transformation and all animators 
					 *  and skip the rest.
					 */
					nd = new Node(Node::DUMMY);
				}

				/* Attach the newly created node to the scenegraph
				 */
				curNode = nd;
				nd->parent = curParent;
				curParent->children.push_back(nd);
			}
			else if (!ASSIMP_stricmp(reader->getNodeName(),"materials"))
			{
				inMaterials = true;
			}
			else if (!ASSIMP_stricmp(reader->getNodeName(),"animators"))
			{
				inAnimator = true;
			}
			else if (!ASSIMP_stricmp(reader->getNodeName(),"attributes"))
			{
				/*  We should have a valid node here
				 */
				if (!curNode)
				{
					DefaultLogger::get()->error("IRR: Encountered <attributes> element, but "
						"there is no node active");
					continue;
				}

				Animator* curAnim = NULL;

				// FIX: Materials can occur for nearly any type of node
				if (inMaterials /* && curNode->type == Node::ANIMMESH ||
					curNode->type == Node::MESH  */)
				{
					/*  This is a material description - parse it!
					 */
					curNode->materials.push_back(std::pair< aiMaterial*, unsigned int > () );
					std::pair< aiMaterial*, unsigned int >& p = curNode->materials.back();

					p.first = ParseMaterial(p.second);

					++guessedMatCnt;
					continue;
				}
				else if (inAnimator)
				{
					/*  This is an animation path - add a new animator
					 *  to the list.
					 */
					curNode->animators.push_back(Animator());
					curAnim = & curNode->animators.back();

					++guessedAnimCnt;
				}

				/*  Parse all elements in the attributes block 
				 *  and process them.
				 */
				while (reader->read())
				{
					if (reader->getNodeType() == EXN_ELEMENT)
					{
						if (!ASSIMP_stricmp(reader->getNodeName(),"vector3d"))
						{
							VectorProperty prop;
							ReadVectorProperty(prop);

							// Convert to our coordinate system
							std::swap( (float&)prop.value.z, (float&)prop.value.y );
							prop.value.y *= -1.f;

							if (inAnimator)
							{
								if (curAnim->type == Animator::ROTATION && prop.name == "Rotation")
								{
									// We store the rotation euler angles in 'direction'
									curAnim->direction = prop.value;
								}
								else if (curAnim->type == Animator::FOLLOW_SPLINE)
								{
									// Check whether the vector follows the PointN naming scheme,
									// here N is the ONE-based index of the point
									if (prop.name.length() >= 6 && prop.name.substr(0,5) == "Point")
									{
										// Add a new key to the list
										curAnim->splineKeys.push_back(aiVectorKey());
										aiVectorKey& key = curAnim->splineKeys.back();

										// and parse its properties
										key.mValue = prop.value;
										key.mTime  = strtol10(&prop.name.c_str()[5]);
									}
								}
								else if (curAnim->type == Animator::FLY_CIRCLE)
								{
									if (prop.name == "Center")
									{
										curAnim->circleCenter = prop.value;
									}
									else if (prop.name == "Direction")
									{
										curAnim->direction = prop.value;

										// From Irrlicht source - a workaround for backward
										// compatibility with Irrlicht 1.1
										if (curAnim->direction == aiVector3D())
										{
											curAnim->direction = aiVector3D(0.f,1.f,0.f);
										}
										else curAnim->direction.Normalize();
									}
								}
								else if (curAnim->type == Animator::FLY_STRAIGHT)
								{
									if (prop.name == "Start")
									{
										// We reuse the field here
										curAnim->circleCenter = prop.value;
									}
									else if (prop.name == "End")
									{
										// We reuse the field here
										curAnim->direction = prop.value;
									}
								}
							}
							else
							{
								if (prop.name == "Position")
								{
									curNode->position = prop.value;
								}
								else if (prop.name == "Rotation")
								{
									curNode->rotation = prop.value;
								}
								else if (prop.name == "Scale")
								{
									curNode->scaling = prop.value;
								}
								else if (Node::CAMERA == curNode->type)
								{
									aiCamera* cam = cameras.back();
									if (prop.name == "Target")
									{
										cam->mLookAt = prop.value;
									}
									else if (prop.name == "UpVector")
									{
										cam->mUp = prop.value;
									}
								}
							}
						}
						else if (!ASSIMP_stricmp(reader->getNodeName(),"bool"))
						{
							BoolProperty prop;
							ReadBoolProperty(prop);

							if (inAnimator && curAnim->type == Animator::FLY_CIRCLE && prop.name == "Loop")
							{
								curAnim->loop = prop.value;
							}
						}
						else if (!ASSIMP_stricmp(reader->getNodeName(),"float"))
						{
							FloatProperty prop;
							ReadFloatProperty(prop);

							if (inAnimator)
							{
								// The speed property exists for several animators
								if (prop.name == "Speed")
								{
									curAnim->speed = prop.value;
								}
								else if (curAnim->type == Animator::FLY_CIRCLE && prop.name == "Radius")
								{
									curAnim->circleRadius = prop.value;
								}
								else if (curAnim->type == Animator::FOLLOW_SPLINE && prop.name == "Tightness")
								{
									curAnim->tightness = prop.value;
								}
							}
							else
							{
								if (prop.name == "FramesPerSecond" &&
									Node::ANIMMESH == curNode->type)
								{
									curNode->framesPerSecond = prop.value;
								}
								else if (Node::CAMERA == curNode->type)
								{	
									/*  This is the vertical, not the horizontal FOV.
									*  We need to compute the right FOV from the
									*  screen aspect which we don't know yet.
									*/
									if (prop.name == "Fovy")
									{
										cameras.back()->mHorizontalFOV  = prop.value;
									}
									else if (prop.name == "Aspect")
									{
										cameras.back()->mAspect = prop.value;
									}
									else if (prop.name == "ZNear")
									{
										cameras.back()->mClipPlaneNear = prop.value;
									}
									else if (prop.name == "ZFar")
									{
										cameras.back()->mClipPlaneFar = prop.value;
									}
								}
								else if (Node::LIGHT == curNode->type)
								{	
									/*  Additional light information
								 */
									if (prop.name == "Attenuation")
									{
										lights.back()->mAttenuationLinear  = prop.value;
									}
									else if (prop.name == "OuterCone")
									{
										lights.back()->mAngleOuterCone =  AI_DEG_TO_RAD( prop.value );
									}
									else if (prop.name == "InnerCone")
									{
										lights.back()->mAngleInnerCone =  AI_DEG_TO_RAD( prop.value );
									}
								}
								// radius of the sphere to be generated -
								// or alternatively, size of the cube
								else if (Node::SPHERE == curNode->type && prop.name == "Radius" ||
										 Node::CUBE == curNode->type   && prop.name == "Size" )
								{
									curNode->sphereRadius = prop.value;
								}
							}
						}
						else if (!ASSIMP_stricmp(reader->getNodeName(),"int"))
						{
							IntProperty prop;
							ReadIntProperty(prop);

							if (inAnimator)
							{
								if (curAnim->type == Animator::FLY_STRAIGHT && prop.name == "TimeForWay")
								{
									curAnim->timeForWay = prop.value;
								}
							}
							else
							{
								// sphere polgon numbers in each direction
								if (Node::SPHERE == curNode->type)
								{
									if (prop.name == "PolyCountX")
									{
										curNode->spherePolyCountX = prop.value;
									}
									else if (prop.name == "PolyCountY")
									{
										curNode->spherePolyCountY = prop.value;
									}
								}
							}
						}
						else if (!ASSIMP_stricmp(reader->getNodeName(),"string") ||
							!ASSIMP_stricmp(reader->getNodeName(),"enum"))
						{
							StringProperty prop;
							ReadStringProperty(prop);
							if (prop.value.length())
							{
								if (prop.name == "Name")
								{
									curNode->name = prop.value;

									/*  If we're either a camera or a light source
									 *  we need to update the name in the aiLight/
									 *  aiCamera structure, too.
									 */
									if (Node::CAMERA == curNode->type)
									{
										cameras.back()->mName.Set(prop.value);
									}
									else if (Node::LIGHT == curNode->type)
									{
										lights.back()->mName.Set(prop.value);
									}
								}
								else if (Node::LIGHT == curNode->type && "LightType" == prop.name)
								{
								}
								else if (prop.name == "Mesh" && Node::MESH == curNode->type ||
									Node::ANIMMESH == curNode->type)
								{
									/*  This is the file name of the mesh - either
									 *  animated or not. We need to make sure we setup
									 *  the correct postprocessing settings here.
									 */
									unsigned int pp = 0;
									BatchLoader::PropertyMap map;

									/* If the mesh is a static one remove all animations
									 */
									if (Node::ANIMMESH != curNode->type)
									{
										pp |= aiProcess_RemoveComponent;
										SetGenericProperty<int>(map.ints,AI_CONFIG_PP_RVC_FLAGS,
											aiComponent_ANIMATIONS | aiComponent_BONEWEIGHTS);
									}

									batch.AddLoadRequest(prop.value,pp,&map);
									curNode->meshPath = prop.value;
								}
								else if (inAnimator && prop.name == "Type")
								{
									// type of the animator
									if (prop.value == "rotation")
									{
										curAnim->type = Animator::ROTATION;
									}
									else if (prop.value == "flyCircle")
									{
										curAnim->type = Animator::FLY_CIRCLE;
									}
									else if (prop.value == "flyStraight")
									{
										curAnim->type = Animator::FLY_CIRCLE;
									}
									else if (prop.value == "followSpline")
									{
										curAnim->type = Animator::FOLLOW_SPLINE;
									}
									else
									{
										DefaultLogger::get()->warn("IRR: Ignoring unknown animator: "
											+ prop.value);

										curAnim->type = Animator::UNKNOWN;
									}
								}
							}
						}
					}
					else if (reader->getNodeType() == EXN_ELEMENT_END &&
							 !ASSIMP_stricmp(reader->getNodeName(),"attributes"))
					{
						break;
					}
				}
			}
			break;

		case EXN_ELEMENT_END:
		
			// If we reached the end of a node, we need to continue processing its parent
			if (!ASSIMP_stricmp(reader->getNodeName(),"node"))
			{
				if (!curNode)
				{
					// currently is no node set. We need to go
					// back in the node hierarchy
					curParent = curParent->parent;
					if (!curParent)
					{
						curParent = root;
						DefaultLogger::get()->error("IRR: Too many closing <node> elements");
					}
				}
				else curNode = NULL;
			}
			// clear all flags
			else if (!ASSIMP_stricmp(reader->getNodeName(),"materials"))
			{
				inMaterials = false;
			}
			else if (!ASSIMP_stricmp(reader->getNodeName(),"animators"))
			{
				inAnimator = false;
			}
			break;

		default:
			// GCC complains that not all enumeration values are handled
			break;
		}
	}

	/*  Now iterate through all cameras and compute their final (horizontal) FOV
	 */
	for (std::vector<aiCamera*>::iterator it = cameras.begin(), end = cameras.end();
		 it != end; ++it)
	{
		aiCamera* cam = *it;
		if (cam->mAspect) // screen aspect could be missing
		{
			cam->mHorizontalFOV *= cam->mAspect;
		}
		else DefaultLogger::get()->warn("IRR: Camera aspect is not given, can't compute horizontal FOV");
	}

	/* Allocate a tempoary scene data structure
	 */
	aiScene* tempScene = new aiScene();
	tempScene->mRootNode = new aiNode();
	tempScene->mRootNode->mName.Set("<IRRRoot>");

	/* Copy the cameras to the output array
	 */
	tempScene->mNumCameras = (unsigned int)cameras.size();
	tempScene->mCameras = new aiCamera*[tempScene->mNumCameras];
	::memcpy(tempScene->mCameras,&cameras[0],sizeof(void*)*tempScene->mNumCameras);

	/* Copy the light sources to the output array
	 */
	tempScene->mNumLights = (unsigned int)lights.size();
	tempScene->mLights = new aiLight*[tempScene->mNumLights];
	::memcpy(tempScene->mLights,&lights[0],sizeof(void*)*tempScene->mNumLights);

	// temporary data
	std::vector< aiNodeAnim*>		anims;
	std::vector< aiMaterial*>		materials;
	std::vector< AttachmentInfo >	attach;
	std::vector<aiMesh*>			meshes;

	// try to guess how much storage we'll need
	anims.reserve     (guessedAnimCnt + (guessedAnimCnt >> 2));
	meshes.reserve    (guessedMeshCnt + (guessedMeshCnt >> 2));
	materials.reserve (guessedMatCnt  + (guessedMatCnt >> 2));

	/* Now process our scenegraph recursively: generate final
	 * meshes and generate animation channels for all nodes.
	 */
	unsigned int defMatIdx = 0xffffffff;
	GenerateGraph(root,tempScene->mRootNode, tempScene,
		batch, meshes, anims, attach, materials, defMatIdx);

	if (!anims.empty())
	{
		tempScene->mNumAnimations = 1;
		tempScene->mAnimations = new aiAnimation*[tempScene->mNumAnimations];
		aiAnimation* an = tempScene->mAnimations[0] = new aiAnimation();

		// ***********************************************************
		// This is only the global animation channel of the scene.
		// If there are animated models, they will have separate 
		// animation channels in the scene. To display IRR scenes
		// correctly, users will need to combine the global anim
		// channel with all the local animations they want to play
		// ***********************************************************
		an->mName.Set("Irr_GlobalAnimChannel");

		// copy all node animation channels to the global channel
		an->mNumChannels = (unsigned int)anims.size();
		an->mChannels = new aiNodeAnim*[an->mNumChannels];
		::memcpy(an->mChannels, & anims [0], sizeof(void*)*an->mNumChannels);
	}
	if (meshes.empty())
	{
		// There are no meshes in the scene - the scene is incomplete
		pScene->mFlags |= AI_SCENE_FLAGS_INCOMPLETE;
		DefaultLogger::get()->info("IRR: No Meshes loaded, setting AI_SCENE_FLAGS_INCOMPLETE flag");
	}
	else
	{
		// copy all meshes to the temporary scene
		tempScene->mNumMeshes = (unsigned int)meshes.size();
		tempScene->mMeshes = new aiMesh*[tempScene->mNumMeshes];
		::memcpy(tempScene->mMeshes,&meshes[0],tempScene->mNumMeshes);
	}

	/*  Now merge all sub scenes and attach them to the correct
	 *  attachment points in the scenegraph.
	 */
	SceneCombiner::MergeScenes(pScene,tempScene,attach);


	/* Finished ... everything destructs automatically and all 
	 * temporary scenes have already been deleted by MergeScenes()
	 */
}
