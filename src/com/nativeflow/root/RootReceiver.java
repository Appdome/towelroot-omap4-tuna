package com.nativeflow.root;

import android.util.Log;
import android.content.Intent;
import android.content.Context;
import android.content.BroadcastReceiver;

import com.nativeflow.root.Root;

public class RootReceiver extends BroadcastReceiver
{
	private final String TAG = "RootReceiver";

	@Override
	public void onReceive(Context context, Intent intent)
	{
		Log.d(TAG, "RootReceiver triggered by " + intent.getAction());
		long status = Root.root();
	}
}
