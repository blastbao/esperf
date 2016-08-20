//
// Created by Kosho Owa on 2016/08/19.
// Worker thread to perform HTTP requests
//


#include "Worker.h"

Worker::Worker(Stats *stats, Options *options) : stats_(stats), options_(options) {}

void Worker::Run() {
    // init easy curl
    CURL *curl = curl_easy_init();

    if (curl) {
        CURLcode cr;
        FILE *f;

        // Do not show the response unless verbose logging
        if (!options_->verbose_) {
            f = fopen("/dev/null", "wb");
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        }

        // Set the method explicitly
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, options_->http_method_.c_str());

        // Capture HTTP errors
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        // Enable basic auth
        if (options_->http_user_.empty()) {
            curl_easy_setopt(curl, CURLOPT_USERPWD, options_->http_user_.c_str());
        }

        for (int i = 0; i < options_->num_recurrence_; i++) {
            // supply random numbers and strings
            string url = ReplaceRNUMEx(options_->request_url_);
            url = ReplaceRNUM(url);
            string query = ReplaceRNUMEx(options_->request_body_);
            query = ReplaceRNUM(query);
            if (options_->dict_.size() > 0) {
                url = ReplaceRDICT(url);
                query = ReplaceRDICT(query);
            }

            // set the URL and the body
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());

            // Perform a request
            cr = curl_easy_perform(curl);

            stringstream msg;

            // curl and HTTP errors
            switch (cr) {
                case CURLE_OK:
                    long sizeUpload;
                    double sizeDownload;
                    long sizeReceivedHeader;
                    double transferTime;
                    curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &sizeUpload);
                    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &sizeDownload);
                    curl_easy_getinfo(curl, CURLINFO_HEADER_SIZE, &sizeReceivedHeader);
                    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &transferTime);
                    stats_->Count(1, 0, 0, sizeUpload, (u_int) (sizeDownload + sizeReceivedHeader),
                                  transferTime);
                    break;
                case CURLE_HTTP_RETURNED_ERROR:
                    long http_response_code;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
                    // TODO: thread safe
                    // msg << "Error: HTTP response (" << http_response_code << ")" << endl;
                    //cerr << msg.str();
                    stats_->Count(0, 0, 1, 0, 0, 0);
                    break;
                default:
                    // TODO: thread safe
                    // msg << "Error: curl_easy_perform() returned (" << cr << ") " << curl_easy_strerror(cr) << endl;
                    //cerr << msg.str();
                    stats_->Count(0, 1, 0, 0, 0, 0);
            }
        }
        fclose(f);
        curl_easy_cleanup(curl);
    }
}

// Replace $RNUM with random numbers
string Worker::ReplaceRNUM(string in) {
    random_device rd;

    string from = "$RNUM";
    string subject = in;
    string::size_type pos = subject.find(from);

    while (pos != string::npos) {
        string to = to_string(rd() % 256);
        subject.replace(pos, from.size(), to);
        pos = subject.find(from, pos + to.size());
    }

    return subject;
}

// Replace $RDICT with randomly selected strings
string Worker::ReplaceRDICT(string in) {
    random_device rd;

    string from = "$RDICT";
    string subject = in;
    string::size_type pos = subject.find(from);

    while (pos != string::npos) {
        string to = options_->dict_.at(rd() % options_->dict_.size());
        subject.replace(pos, from.size(), to);
        pos = subject.find(from, pos + to.size());
    }

    return subject;
}

string Worker::ReplaceRNUMEx(string in) {
    random_device rd;

    string from = "$RNUM(";
    string subject = in;
    string::size_type s_pos = subject.find(from);

    while (s_pos != string::npos) {
        string::size_type e_pos = subject.find(")", s_pos);
        int m = atoi(subject.substr(s_pos + from.size(), e_pos).c_str());
        string to;
        if (m > 0) {
            to = to_string(rd() % m);
        }else{
            to = "";
        }
        subject.replace(s_pos, e_pos - s_pos +1, to);
        s_pos = subject.find(from, s_pos + to.size());
    }

    return subject;
}