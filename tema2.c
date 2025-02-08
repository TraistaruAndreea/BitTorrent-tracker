#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define MSG_INIT_FILES 1
#define MSG_ACK 2
#define MSG_REQUEST_SWARM 3
#define MSG_SWARM_RESPONSE 4
#define MSG_CHUNK_REQUEST 5
#define MSG_CHUNK_RESPONSE 6
#define MSG_ALL_COMPLETE 7
#define MSG_SHUTDOWN 8
#define MSG_FILE_DOWNLOADED 9

typedef struct{
    char filename[MAX_FILENAME];
    int num_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 1];
} File;

typedef struct{
    File owned_files[MAX_FILES];
    int num_owned_files;
    char wanted_files[MAX_FILES][MAX_FILENAME];
    int num_wanted_files;
    bool should_stop;
    int chunks_downloaded;
    bool all_files_downloaded;
    pthread_mutex_t mutex;
} ClientData;

typedef struct {
    int peer_rank;
    bool isPeer;
} PeerData;

typedef struct{
    char filename[MAX_FILENAME];
    int num_chunks;
    PeerData peers[MAX_FILES];
    int num_peers;
} SwarmInfo;

ClientData *client_data;

void initialize_client_data() {
    client_data = (ClientData *)malloc(sizeof(ClientData));
    if (!client_data) {
        fprintf(stderr, "Failed to allocate memory for client_data\n");
        exit(-1);
    }
    memset(client_data, 0, sizeof(ClientData));
    pthread_mutex_init(&client_data->mutex, NULL);
}

void cleanup_client_data() {
    if (client_data) {
        pthread_mutex_destroy(&client_data->mutex);
        free(client_data);
        client_data = NULL;
    }
}

void read_input_file(int rank, ClientData *client_data) {
    char filename[MAX_FILENAME];
    sprintf(filename, "in%d.txt", rank);
    FILE *input = fopen(filename, "r");
    if (!input) {
        printf("Error opening input file for rank %d\n", rank);
        exit(1);
    }

    fscanf(input, "%d", &client_data->num_owned_files);
    for (int i = 0; i < client_data->num_owned_files; i++) {
        fscanf(input, "%s %d", client_data->owned_files[i].filename,
               &client_data->owned_files[i].num_chunks);
        for (int j = 0; j < client_data->owned_files[i].num_chunks; j++) {
            fscanf(input, "%s", client_data->owned_files[i].chunks[j]);
        }
    }

    // Send owned files to tracker
    MPI_Send(&client_data->num_owned_files, 1, MPI_INT, TRACKER_RANK, MSG_INIT_FILES, MPI_COMM_WORLD);
    for (int i = 0; i < client_data->num_owned_files; i++) {
        MPI_Send(client_data->owned_files[i].filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, MSG_INIT_FILES, MPI_COMM_WORLD);
        MPI_Send(&client_data->owned_files[i].num_chunks, 1, MPI_INT, TRACKER_RANK, MSG_INIT_FILES, MPI_COMM_WORLD);
    }

    fscanf(input, "%d", &client_data->num_wanted_files);
    for (int i = 0; i < client_data->num_wanted_files; i++) {
        fscanf(input, "%s", client_data->wanted_files[i]);
    }

    fclose(input);
}

void* download_thread_func(void* arg) {
    int rank = *(int*)arg;

    int ack;
    MPI_Recv(&ack, 1, MPI_INT, TRACKER_RANK, MSG_ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int i = 0;
    client_data->chunks_downloaded = 0;
    while(true) {
        if (client_data->num_wanted_files == 0) {
            break;
        }
        pthread_mutex_lock(&client_data->mutex);
        if (client_data->should_stop == true) {
            pthread_mutex_unlock(&client_data->mutex);
            return NULL;
        }
        pthread_mutex_unlock(&client_data->mutex);

        MPI_Send(client_data->wanted_files[i], MAX_FILENAME, MPI_CHAR, TRACKER_RANK, MSG_REQUEST_SWARM, MPI_COMM_WORLD);
        
        SwarmInfo swarm;
        printf ("Inainte recv swarm afara while\n");
        MPI_Recv(&swarm.num_chunks, 1, MPI_INT, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&swarm.num_peers, 1, MPI_INT, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf ("Dupa recv swarm afara while\n");
        for (int j = 0; j < swarm.num_peers; j++) {
            pthread_mutex_lock(&client_data->mutex);
            if (client_data->should_stop == true) {
                pthread_mutex_unlock(&client_data->mutex);
                return NULL;
            }
            pthread_mutex_unlock(&client_data->mutex);
            MPI_Recv(&swarm.peers[j], sizeof(PeerData), MPI_BYTE, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        char filename[MAX_FILENAME * 2];
        sprintf(filename, "client%d_%s", rank, client_data->wanted_files[i]);
        FILE *output = fopen(filename, "w");
        if (!output) {
            printf("Error creating output file %s\n", filename);
            return 0;
        }

        int chunk = 0;
        int peer_index = 0;
        int peer_rank;
        while (chunk < swarm.num_chunks) {
            pthread_mutex_lock(&client_data->mutex);
            if (client_data->should_stop == true) {
                pthread_mutex_unlock(&client_data->mutex);
                return NULL;
            }
            pthread_mutex_unlock(&client_data->mutex);

            peer_rank = swarm.peers[peer_index].peer_rank;
            MPI_Send(&client_data->wanted_files[i], MAX_FILENAME, MPI_CHAR, peer_rank, MSG_CHUNK_REQUEST, MPI_COMM_WORLD);
            MPI_Send(&chunk, 1, MPI_INT, peer_rank, MSG_CHUNK_REQUEST, MPI_COMM_WORLD);

            char response[3];
            MPI_Recv(response, 3, MPI_CHAR, peer_rank, MSG_CHUNK_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            pthread_mutex_lock(&client_data->mutex);
            if (client_data->should_stop == true) {
                pthread_mutex_unlock(&client_data->mutex);
                return NULL;
            }
            pthread_mutex_unlock(&client_data->mutex);
            if (strcmp(response, "OK") == 0) {
                chunk++;
                char chunk_data[HASH_SIZE + 1];
                MPI_Recv(chunk_data, HASH_SIZE + 1, MPI_CHAR, peer_rank, MSG_CHUNK_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                strcpy(client_data->owned_files[client_data->num_owned_files + 1].chunks[chunk - 1], chunk_data);
                fprintf(output, "%s\n", chunk_data);

                if (chunk % 10 == 0) {
                    MPI_Send(client_data->wanted_files[i], MAX_FILENAME, MPI_CHAR, TRACKER_RANK, MSG_REQUEST_SWARM, MPI_COMM_WORLD);

                    MPI_Recv(&swarm.num_chunks, 1, MPI_INT, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&swarm.num_peers, 1, MPI_INT, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    for (int j = 0; j < swarm.num_peers; j++) {
                        pthread_mutex_lock(&client_data->mutex);
                        if (client_data->should_stop == true) {
                            pthread_mutex_unlock(&client_data->mutex);
                            return NULL;
                        }
                        pthread_mutex_unlock(&client_data->mutex);
                        MPI_Recv(&swarm.peers[j], sizeof(PeerData), MPI_BYTE, TRACKER_RANK, MSG_SWARM_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    printf("Dupa recv swarm in while\n");
                }
            } 
            peer_index = (peer_index + 1) % swarm.num_peers;
            if (peer_index == swarm.num_peers - 1)  peer_index = 0;
        }

        fclose(output);
        if (chunk == swarm.num_chunks) {
            i++;
            client_data->chunks_downloaded++;
            client_data->num_owned_files++;
            client_data->owned_files[client_data->num_owned_files].num_chunks = swarm.num_chunks;
            strcpy(client_data->owned_files[client_data->num_owned_files].filename, client_data->wanted_files[i]);
            MPI_Send(&rank, 1, MPI_INT, TRACKER_RANK, MSG_FILE_DOWNLOADED, MPI_COMM_WORLD);
        }
        if (client_data->chunks_downloaded == client_data->num_wanted_files) {
            break;
        }
    }

    pthread_mutex_lock(&client_data->mutex);
    if (client_data->chunks_downloaded == client_data->num_wanted_files && client_data->all_files_downloaded == false) {
        client_data->all_files_downloaded = true;
        pthread_mutex_unlock(&client_data->mutex);
        MPI_Send(&rank, 1, MPI_INT, TRACKER_RANK, MSG_ALL_COMPLETE, MPI_COMM_WORLD);
    }
    pthread_mutex_unlock(&client_data->mutex);
    return NULL;
}

void* upload_thread_func(void* arg) {
    int rank = *(int*)arg;
    MPI_Status status;

    while (true) {
        pthread_mutex_lock(&client_data->mutex);
        if (client_data->should_stop) {
            pthread_mutex_unlock(&client_data->mutex);
            return NULL;
        }
        pthread_mutex_unlock(&client_data->mutex);

        // Use a non-blocking receive to check for messages while allowing shutdown
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, MSG_CHUNK_REQUEST, MPI_COMM_WORLD, &flag, &status);

        if (flag) {
            int source_rank = status.MPI_SOURCE;

            // Probe and allocate a buffer for the filename
            MPI_Probe(source_rank, MSG_CHUNK_REQUEST, MPI_COMM_WORLD, &status);
            int filename_length;
            MPI_Get_count(&status, MPI_CHAR, &filename_length);

            char* requested_file = (char*)malloc(filename_length + 1);
            if (!requested_file) {
                fprintf(stderr, "Memory allocation failed for requested_file\n");
                continue;
            }

            MPI_Recv(requested_file, filename_length, MPI_CHAR, source_rank, MSG_CHUNK_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            requested_file[filename_length] = '\0';

            int requested_chunk;
            MPI_Recv(&requested_chunk, 1, MPI_INT, source_rank, MSG_CHUNK_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            bool file_found = false;

            // Find the requested file and chunk
            for (int i = 0; i < client_data->num_owned_files; i++) {
                pthread_mutex_lock(&client_data->mutex);
                if (client_data->should_stop) {
                    pthread_mutex_unlock(&client_data->mutex);
                    free(requested_file);
                    return NULL;
                }
                pthread_mutex_unlock(&client_data->mutex);

                if (strcmp(client_data->owned_files[i].filename, requested_file) == 0) {
                    if (requested_chunk >= 0 && requested_chunk < client_data->owned_files[i].num_chunks) {
                        file_found = true;
                        MPI_Send("OK", 3, MPI_CHAR, source_rank, MSG_CHUNK_RESPONSE, MPI_COMM_WORLD);
                        MPI_Send(client_data->owned_files[i].chunks[requested_chunk], HASH_SIZE + 1, MPI_CHAR, source_rank, MSG_CHUNK_RESPONSE, MPI_COMM_WORLD);
                        break;
                    }
                }
            }

            if (!file_found) {
                MPI_Send("NO", 3, MPI_CHAR, source_rank, MSG_CHUNK_RESPONSE, MPI_COMM_WORLD);
            }

            free(requested_file);
        }
    }
    return NULL;
}

void tracker(int numtasks, int rank) {
    SwarmInfo swarms[MAX_FILES];
    int num_swarms = 0;
    int completed_clients = 0;

    // Collect file information from clients
    for (int i = 1; i < numtasks; i++) {
        int num_files;
        MPI_Recv(&num_files, 1, MPI_INT, i, MSG_INIT_FILES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


        for (int j = 0; j < num_files; j++) {
            File received_file;
            MPI_Recv(received_file.filename, MAX_FILENAME, MPI_CHAR, i, MSG_INIT_FILES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&received_file.num_chunks, 1, MPI_INT, i, MSG_INIT_FILES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Add to swarm if new file
            bool found = false;
            for (int k = 0; k < num_swarms; k++) {
                if (strcmp(swarms[k].filename, received_file.filename) == 0) {
                    swarms[k].peers[swarms[k].num_peers++].peer_rank = i;
                    swarms[k].peers[swarms[k].num_peers].isPeer = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                strcpy(swarms[num_swarms].filename, received_file.filename);
                swarms[num_swarms].num_chunks = received_file.num_chunks;
                swarms[num_swarms].peers[0].peer_rank = i;
                swarms[num_swarms].peers[0].isPeer = true;
                swarms[num_swarms].num_peers = 1;
                num_swarms++;
            }
        }
    }

    // Acknowledge all clients
    for (int i = 1; i < numtasks; i++) {
        int ack = 1;
        MPI_Send(&ack, 1, MPI_INT, i, MSG_ACK, MPI_COMM_WORLD);
    }

    while (completed_clients < numtasks - 1) {
        MPI_Status status;
        char filename[MAX_FILENAME];

        MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == MSG_REQUEST_SWARM) {
            for (int i = 0; i < num_swarms; i++) {
                if (strcmp(swarms[i].filename, filename) == 0) {
                    MPI_Send(&swarms[i].num_chunks, 1, MPI_INT, status.MPI_SOURCE, MSG_SWARM_RESPONSE, MPI_COMM_WORLD);
                    MPI_Send(&swarms[i].num_peers, 1, MPI_INT, status.MPI_SOURCE, MSG_SWARM_RESPONSE, MPI_COMM_WORLD);
                    for (int j = 0; j < swarms[i].num_peers; j++) {
                        MPI_Send(&swarms[i].peers[j], sizeof(PeerData), MPI_BYTE, status.MPI_SOURCE, MSG_SWARM_RESPONSE, MPI_COMM_WORLD);
                    }
                    break;
                }
            }
        } else if (status.MPI_TAG == MSG_FILE_DOWNLOADED) {
            for (int i = 0; i < num_swarms; i++) {
                for (int j = 0; j < swarms[i].num_peers; j++) {
                    if (swarms[i].peers[j].peer_rank == status.MPI_SOURCE) {
                        swarms[i].peers[j].isPeer = false;
                    }
                }
            }
        } else if (status.MPI_TAG == MSG_ALL_COMPLETE) {
            completed_clients++;
           // printf("Client %d completed\n", status.MPI_SOURCE);
        }
    }

   // printf("All clients completed. Sending shutdown signal.\n");
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&i, 1, MPI_INT, i, MSG_SHUTDOWN, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    initialize_client_data();

    client_data->should_stop = false;
    client_data->chunks_downloaded = 0;
    client_data->all_files_downloaded = false;
    
    read_input_file(rank, client_data);
    
    r = pthread_create(&download_thread, NULL, download_thread_func, (void *)&rank);
    if (r) {
        printf("Error creating download thread\n");
        cleanup_client_data();
        exit(-1);
    }
    
    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *)&rank);
    if (r) {
        printf("Error creating upload thread\n");
        cleanup_client_data();
        exit(-1);
    }

    MPI_Status status_tracker;
    int shutdown;
    MPI_Recv(&shutdown, 1, MPI_INT, TRACKER_RANK, MSG_SHUTDOWN, MPI_COMM_WORLD, &status_tracker);
    if (status_tracker.MPI_TAG == MSG_SHUTDOWN) {
        pthread_mutex_lock(&client_data->mutex);
        client_data->should_stop = true;
        pthread_mutex_unlock(&client_data->mutex);
    }
    // printf("Client %d received shutdown\n", rank);

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Error joining download thread\n");
        cleanup_client_data();
        exit(-1);
    }
    // printf("Client %d finished download\n", rank);

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Error joining upload thread\n");
        cleanup_client_data();
        exit(-1);
    }
    // printf("Client %d finished upload\n", rank);

    cleanup_client_data();
}

int main(int argc, char *argv[]) {
    int numtasks, rank;
    int provided;
    
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }
    MPI_Finalize();
}