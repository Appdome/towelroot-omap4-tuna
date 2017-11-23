package com.nativeflow.root;

import java.io.File;
import java.net.Socket;
import java.io.IOException;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.UnknownHostException;

import android.util.Log;
import android.os.Bundle;
import android.os.AsyncTask;
import android.app.Activity;
import android.widget.Button;
import android.content.Intent;
import android.content.Context;
import android.content.ComponentName;

import com.nativeflow.root.Root;

public class RootMain extends Activity
{
	private static String TAG = "RootMain";

	private Socket mSocket = null;
	private DataInputStream mSockInput = null;
	private DataOutputStream mSockOutput = null;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

		/* Try to connect to the daemon */
		if (tryToConnect() == -1) {
			Log.d(TAG, "Failed to connect to daemon. Launching it.");
			long status = Root.root();
			if (status == 0) {
				new ConnectToDaemonTask().execute(5);
			} else {
				Log.d(TAG, "Root failed");
			}
		} else {
			doRootMe();
		}
    }

	private void doRootMe()
	{
		int pid = android.os.Process.myPid();
		if (!isConnectionEstablished()) {
			/* Something terrible happened, we're not supposed to be here */
			Log.e(TAG, "Execution reached doRootMe, but connection has "
					+ "not been established yet");
			return;
		}
		Log.d(TAG, "About to ask the daemon to root us");
		try {
			/* Send the command */
			mSockOutput.writeInt(pid);
			Log.d(TAG, "Sent command ROOT_ME");
			/* Read the status */
			int status = mSockInput.readInt();
			if (status == 0) {
				/* This process and its thread groups have root access and full
				 * capabilities, we can now do whatever we want. Switch over to
				 * the ApplicationList activity */
				Intent intent = new Intent();
				intent.setComponent(new ComponentName("com.nativeflow.root",
							"com.nativeflow.root.ApplicationList"));
				startActivity(intent);
			} else {
				/* Touching-up this process' credentials failed, display the
				 * appropriate error */
				Log.e(TAG, "RootMe failed with error " + status);
			}
		} catch (IOException e) {
			Log.e(TAG, "Socket I/O error");
			e.printStackTrace();
		}
	}

	private boolean isConnectionEstablished()
	{
		return (mSockInput != null) && (mSockOutput != null);
	}

	private int tryToConnect()
	{
		try {
			mSocket = new Socket("127.0.0.1", 6666);
			mSockInput = new DataInputStream(mSocket.getInputStream());
			mSockOutput = new DataOutputStream(mSocket.getOutputStream());
			return 0;
		} catch (UnknownHostException e) {
			Log.e("Root", "UnknownHostException");
			e.printStackTrace();
			mSocket = null;
		} catch (IOException e) {
			Log.e("Root", "IOException");
			e.printStackTrace();
			mSocket = null;
			mSockInput = null;
			mSockOutput = null;
		}
		return -1;
	}

	private class ConnectToDaemonTask extends AsyncTask<Integer, Integer, Long>
	{
		protected Long doInBackground(Integer... args)
		{
			int retries = 0;
			int maxRetries = 5; /* The default value */
			if (args.length == 1) {
				maxRetries = args[0];
			}
			do {
				Log.i("Root", "Connection attempt #" + retries);
				if (tryToConnect() == 0) {
					return (long)0;
				}
				retries++;
				Log.i("Root", "Retrying connection in 1 second");
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {
					Log.i("Root", "Thread.sleep interrupted");
				}
			} while (!isConnectionEstablished() && (retries < maxRetries));

			return (long)-1;
		}

		protected void onProgressUpdate(Integer... progress)
		{
		}

		protected void onPostExecute(Long result)
		{
			if (result == 0) {
				/* Success: We can ask the daemon to root us */
				doRootMe();
			} else {
				/* Failure: Show some error message? So much for a covert
				 * ponie */
			}
		}
	}
}
