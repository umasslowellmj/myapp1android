package com.munkh.myapp1;

import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.VideoView;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button btv = (Button) findViewById(R.id.b1);
        getWindow().setFormat(PixelFormat.UNKNOWN);
        VideoView mView2 = (VideoView) findViewById(R.id.vidView);


        String uriPath2 = "android.resource://" + getPackageName() + "/" + R.raw.funny;

        Uri uri2 = Uri.parse(uriPath2);
        mView2.setVideoURI(uri2);
        mView2.requestFocus();
        mView2.start();

        btv.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                VideoView mView2 = (VideoView) findViewById(R.id.vidView);

                String uriPath2 = "android.resource://" + getPackageName() + "/" + R.raw.funny;
                Uri uri2 = Uri.parse(uriPath2);
                mView2.setVideoURI(uri2);
                mView2.requestFocus();
                mView2.start();
            }
        });


    }

    @Override
    protected void onStart()
    {super.onStart();
        Log.d("onStart", "onStart()");
    }

    @Override
    protected void onPause()
    {super.onPause();
        Log.d("onPause","onPause()");
    }

    @Override
    protected void onResume()
    {super.onResume();
        Log.d("onResume","onResume()");
    }

    @Override
    protected void onStop()
    {super.onStop();
        Log.d("onStop","onStop()");
    }

    @Override
    protected void onDestroy()
    {super.onDestroy();
        Log.d("onDestroy","onDestroy()");
    }

    @Override
    protected void onRestart()
    {super.onRestart();
        Log.d("onRestart","onRestart()");
    }
}

