package org.mozilla.samples.sync

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.samples.sync.DeviceFragment.OnDeviceListInteractionListener
import kotlinx.android.synthetic.main.fragment_device.view.*
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType

/**
 * [RecyclerView.Adapter] that can display a [DummyItem] and makes a call to the
 * specified [OnDeviceListInteractionListener].
 */
class DeviceRecyclerViewAdapter(
    var mListenerDevice: OnDeviceListInteractionListener?
) : RecyclerView.Adapter<DeviceRecyclerViewAdapter.ViewHolder>() {

    val devices = mutableListOf<Device>()

    private val mOnClickListener: View.OnClickListener

    init {
        mOnClickListener = View.OnClickListener { v ->
            val item = v.tag as Device
            // Notify the active callbacks interface (the activity, if the fragment is attached to
            // one) that an item has been selected.
            mListenerDevice?.onDeviceInteraction(item)
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.fragment_device, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = devices[position]
        holder.nameView.text = item.displayName
        holder.typeView.text = when (item.deviceType) {
            DeviceType.DESKTOP -> "Desktop"
            DeviceType.MOBILE -> "Mobile"
            DeviceType.UNKNOWN -> "Unknown"
        }

        with(holder.mView) {
            tag = item
            setOnClickListener(mOnClickListener)
        }
    }

    override fun getItemCount(): Int = devices.size

    inner class ViewHolder(val mView: View) : RecyclerView.ViewHolder(mView) {
        val nameView: TextView = mView.device_name
        val typeView: TextView = mView.device_type
    }
}
