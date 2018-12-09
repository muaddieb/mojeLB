﻿
#include "Solver.h"


void Solver::Collisions() {
#pragma omp parallel for
    for (auto x = 0; x < static_cast<int>(mesh.size()); ++x) {
        for (unsigned int y = 0; y < mesh[x].size(); ++y) {
            mesh[x][y]->ComputeRho();
            mesh[x][y]->ComputeU();
            mesh[x][y]->ComputefEq();

            /*
            mesh[x][y]->CalcEddyViscosity(mycase->bcValues_.nu);
            omegaNSTurb = 1. / (3 * (mycase->bcValues_.nu + mesh[x][y]->nuTurb) + 0.5);
            mesh[x][y]->NodeCollisionFout(omegaNSTurb);
            */

            mesh[x][y]->NodeCollisionFout(omegaNS);

            //thermal stuff:
            mesh[x][y]->ComputeT();
            mesh[x][y]->ComputeTeq();
            mesh[x][y]->NodeCollisionTout(omegaT);
        }
    }
}


void Solver::Stream() {
#pragma omp parallel for
    for (int x = 0; x < mesh.size(); ++x) {
        for (unsigned int y = 0; y < mesh[x].size(); ++y) {
            StreamToNeighbour(x, y);
        }
    }
}

void Solver::StreamToNeighbour(const int &x, const int &y) {
    unsigned nextX, nextY;
    for (unsigned i = 0; i < d2q9Constants->ee.size(); ++i) {
        nextX = x + static_cast<int>(d2q9Constants->ee[i](0));
        nextY = y + static_cast<int>(d2q9Constants->ee[i](1));

        if (nextX >= 0 && nextX < mesh.size() && nextY >= 0 && nextY < mesh[x].size())
            mesh[nextX][nextY]->fIn[i] = mesh[x][y]->fOut[i];
    }

    //thermal stuff:
    for (unsigned i = 0; i < d2q5Constants->ee.size(); ++i) //Passive Scalar
    {
        nextX = x + static_cast<int>(d2q5Constants->ee[i](0));
        nextY = y + static_cast<int>(d2q5Constants->ee[i](1));

        if (nextX >= 0 && nextX < mesh.size() && nextY >= 0 && nextY < mesh[x].size())
            mesh[nextX][nextY]->TIn[i] = mesh[x][y]->TOut[i];
    }


    //periodic BC
    if (mesh[x][y]->nodeType == NodeType::PeriodicType) {
        if (x == mesh.size() - 1)    // periodic boundary conditions for east moving particles at east end
        {
            nextX = 0;
            mesh[nextX][y]->fIn[5] = mesh[x][y]->fOut[5];
            mesh[nextX][y]->fIn[1] = mesh[x][y]->fOut[1];
            mesh[nextX][y]->fIn[8] = mesh[x][y]->fOut[8];

            mesh[nextX][y]->TIn[1] = mesh[x][y]->TOut[1];
        } else if (x == 0) // periodic boundary conditions for west moving particles at west end
        {
            nextX = mesh.size() - 1;
            mesh[nextX][y]->fIn[6] = mesh[0][y]->fOut[6];
            mesh[nextX][y]->fIn[3] = mesh[0][y]->fOut[3];
            mesh[nextX][y]->fIn[7] = mesh[0][y]->fOut[7];

            mesh[nextX][y]->TIn[3] = mesh[0][y]->TOut[3];
        } else
            throw std::runtime_error("not implemented");

    }
}


void Solver::Run() {


    boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
    string nowAsString = to_iso_string(now);

    const string fileNameVTK = "mojeLB";
    string outputDirectory = "output";

    writer->ClearDirectory(outputDirectory);
    writer->WriteCaseInfo(mycase, outputDirectory, "caseInfoData");

    //outputDirectory += "/" + nowAsString;
    vector<double> LidShearForce;

    unsigned t = 0;
    while (t < mycase->timer_.totalTime) {
        /*	if (t%50 == 0)
            cout << "time: " << t << endl;*/

        //cout << "average T: " << this->GetAverageT() << endl;
        if (t % mycase->timer_.timeToSavePointData == 0) {
            cout << endl;
            cout << "calculating time step: " << t << endl;
            //	writer->writePointData(mesh, t, mycase->meshGeom_.x / 2, mycase->meshGeom_.y / 2, outputDirectory, "PointData");
            //	writer->WriteCrossSectionData(mesh, t, outputDirectory, "CrossSectionData");
            //	cout << "Average Temp: " << this->GetAverageT() << endl;

            //writer->WriteScalar(GetNWallShearForce(), t, outputDirectory, "NWallShearForce");
            //cout << "---------przed kolizja------------" << endl;
            //GetNWallShearForce();
        }

//            cout << "calculating time step: " << t << endl;
//            this->IsDensityValid();



        if (t % mycase->timer_.timeToSaveVTK == 0) {
            try { this->IsDensityValid(); }
            catch (exception &e) {
                cout << "Solver exception: " << e.what() << endl;
                break;
            }
            vector<vector<shared_ptr<Node>>> tempMesh = std::move(this->CloneMesh());
            std::thread writingThread(&VTKWriter::writeVTK, writer, tempMesh, t, outputDirectory, fileNameVTK);
            writingThread.detach();

            //old way
//            writer->writeVTK(mesh, t, outputDirectory, fileNameVTK);
        }

        this->Collisions();

        //if (t% mycase->timer_.timeToSavePointData == 0) {
        //cout << "---------po kolizji------------" << endl;
        //GetNWallShearForce();
        //cout << endl;
        //}

        this->Stream();
        ++t;
    }

    //std::vector<std::thread> threadList;
    //for (int i = 0; i < 10; i++)
    //{
    //	threadList.push_back(std::thread(WorkerThread()));
    //}

    //   std::for_each(threadList.begin(),threadList.end(), std::mem_fn(&std::thread::join));
    //   threadList.clear();

}

vector<vector<shared_ptr<Node>>> Solver::CloneMesh() {
    vector<vector<shared_ptr<Node>>> tempMesh(this->mesh); // make a shallow copy

#pragma omp parallel for
    for (int x = 0; x < mesh.size(); ++x) {
        for (int y = 0; y < mesh[x].size(); ++y) {
            //tempMesh[x][y] = mesh[x][y]->CloneShrPtr(); // change objects // todo
            tempMesh[x][y] = std::move(mesh[x][y]->CloneShrPtr()); // change objects
        }
    }

    return tempMesh;
}

double Solver::GetAverageT() {
    double Ttotal = 0;

    for (auto &inner : mesh) {
        for (auto &node : inner) {
            Ttotal += node->T;
        }
    }
    //return Ttotal;
    return Ttotal / mycase->meshGeom_.numberOfNodes;
}

double Solver::GetVarT() {
    double VarT = 0;
    double Tavg = this->GetAverageT();

    for (auto &inner : mesh) {
        for (auto &node : inner) {
            VarT += (node->T - Tavg) * (node->T - Tavg);
        }
    }
    return VarT;
}

double Solver::GetNWallShearForce() {
    double lidShearForce;
    Eigen::Vector2d lidForce_vector(0, 0);

    double bottomShearForce;
    Eigen::Vector2d bottomForce_vector(0, 0);

    Eigen::Vector2d Xdirection(1, 0);
    double yTop = mesh[0].size() - 1;

    //#pragma omp parallel for  reduction(+:lidForce_vector)
    for (int x = 0; x < mesh.size(); ++x) {
        // lid
        //for (unsigned i = 0; i < d2q9Constants->ee.size(); ++i)
        //{
        //	lidForce_vector += (mesh[x][yTop]->fOut[i] * d2q9Constants->ee[i] + mesh[x][yTop]->fOut[i] * d2q9Constants->ee[i]);
        //	lidForce_vector -= (mesh[x][yTop]->fIn[i] * d2q9Constants->ee[i] + mesh[x][yTop]->fIn[i] * d2q9Constants->ee[i]);
        //}

        lidForce_vector += (mesh[x][yTop]->fOut[7] * d2q9Constants->ee[7] +
                            mesh[x][yTop]->fOut[8] * d2q9Constants->ee[8]);
        lidForce_vector -= (mesh[x][yTop]->fIn[5] * d2q9Constants->ee[5] + mesh[x][yTop]->fIn[6] * d2q9Constants->ee[6]);

        //lidForce_vector += (mesh[x][yTop]->fOut[1] * d2q9Constants->ee[1] + mesh[x][yTop]->fOut[3] * d2q9Constants->ee[3]);
        //lidForce_vector -= (mesh[x][yTop]->fIn[1] * d2q9Constants->ee[1] + mesh[x][yTop]->fIn[3] * d2q9Constants->ee[3]);

        //bottom wall y = 0;
        //for (unsigned i = 0; i < d2q9Constants->ee.size(); ++i)
        //{
        //	bottomForce_vector += (mesh[x][0]->fOut[i] * d2q9Constants->ee[i] + mesh[x][0]->fOut[i] * d2q9Constants->ee[i]);
        //	bottomForce_vector -= (mesh[x][0]->fIn[i] * d2q9Constants->ee[i] + mesh[x][0]->fIn[i] * d2q9Constants->ee[i]);
        //}
        bottomForce_vector += (mesh[x][0]->fOut[5] * d2q9Constants->ee[5] +
                               mesh[x][0]->fOut[6] * d2q9Constants->ee[6]); //po odbiciu
        bottomForce_vector -= (mesh[x][0]->fIn[7] * d2q9Constants->ee[7] +
                               mesh[x][0]->fIn[8] * d2q9Constants->ee[8]); //incoming
    }

    lidShearForce = lidForce_vector.dot(Xdirection);
    bottomShearForce = bottomForce_vector.dot(Xdirection);

    cout << "LID sila " << lidShearForce << endl;
    cout << "BOTTOM WALL sila " << bottomShearForce << endl;

    return lidShearForce;
}

shared_ptr<Node> Solver::GetNode(const int &x, const int &y) {
    return mesh[x][y];
}

void Solver::IsDensityValid() {
    for (auto &inner : mesh) {
        for (auto &node : inner) {
            if (node->rho > 1000) {
                string oups = "to big rho = " + std::to_string(node->rho);
                throw std::runtime_error(oups.c_str());
            }
        }
    }

}

void Solver::ReplaceNode(const int &x, const int &y, shared_ptr<Node> newNode) {
    //*mesh[x][y] = newNode;
    mesh[x][y] = std::move(newNode); // TODO: move vs reset?
    //mesh[x][y].reset(newNode);
}



//void Solver::ReplaceNode(const int& x, const int& y, Node newNode) 
//{
//	*mesh[x][y] = newNode;
//	*mesh[x][y] = std::move(newNode); // TODO: which one is better?
//	//mesh[x][y].reset(newNode);
//}


Solver::~Solver() {
    //delete mycase;
    delete d2q5Constants;
    delete d2q9Constants;
}

Solver::Solver() : writer(new VTKWriter) {
    ChannelMeshBuilder channel_mesh_builder("../KarmanCase.xml");
    LidMeshBuilder lid_mesh_builder("../LidDrivenCavity.xml");

    DiffusionValidatorMeshBuilder diffusion_validator_mesh_builder("../DefaultCase.xml");
    AdvectionValidatorMeshBuilder advection_validator_mesh_builder("../DefaultCase.xml");
    WallForceValidatorMeshBuilder wall_force_validator_mesh_builder("../WallForceWalidatorCase.xml");


    meshDirector.SetBuilder(&lid_mesh_builder);

    //meshDirector.SetBuilder(&wall_force_validator_mesh_builder);
    //meshDirector.SetBuilder(&diffusion_validator_mesh_builder);
    //meshDirector.SetBuilder(&advection_validator_mesh_builder);

//    meshDirector.SetBuilder(&channel_mesh_builder);

    mesh = meshDirector.MakeMesh();

    mycase = meshDirector.GetCase();
    omegaNS = 1. / (3 * mycase->bcValues_.nu + 0.5);      //NS relaxation parameter
    omegaT = 1. / (3 * mycase->passive_scalar_blobb_.K + 0.5); //Passive Scalar relaxation parameter
    cout << "omegaNS = " << omegaNS << endl;
    cout << "omegaT = " << omegaT << endl;

    d2q9Constants = Singleton<D2Q9Constants>::get_instance();
    d2q5Constants = Singleton<D2Q5Constants>::get_instance();

    //auto constants_test = DConstantsTest::get_instance();
    //constants_test->InitializeMe();
    //constants_test->wtest[0] = 666;
    //cout << constants_test->wtest[0] << endl;
}

